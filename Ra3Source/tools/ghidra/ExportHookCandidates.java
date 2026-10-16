// Exports static properties used to select safe inline-hook candidates.
// Usage: ExportHookCandidates.java <output.tsv> <function_ledger.tsv>
//@category Ra3Source

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class ExportHookCandidates extends GhidraScript {
    private static final int DECOMPILE_TIMEOUT_SECONDS = 10;
    private static final int MAX_STRINGS = 3;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: ExportHookCandidates.java <output.tsv> <function_ledger.tsv>");
        }
        Path output = Paths.get(args[0]);
        Set<Long> cleanText = readCleanText(Paths.get(args[1]));
        Files.createDirectories(output.toAbsolutePath().getParent());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int written = 0;
        try (PrintWriter out = new PrintWriter(
                Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
            out.println(String.join("\t",
                "entry", "size", "instruction_count", "return_forms",
                "internal_callee_count", "internal_callees", "external_callees",
                "unresolved_calls",
                "global_writes", "global_reads", "uses_fs", "uses_x87", "uses_sse",
                "has_loops", "direct_callers", "strings", "parameter_count",
                "calling_convention", "op_profile"));
            for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                if (!cleanText.contains(function.getEntryPoint().getOffset())) {
                    continue;
                }
                out.println(describe(function, decompiler));
                written++;
                if ((written % 1000) == 0) {
                    println("exported " + written + " / " + cleanText.size());
                }
            }
        } finally {
            decompiler.dispose();
        }
        println("Exported " + written + " clean .text functions to " + output.toAbsolutePath());
    }

    private String describe(Function function, DecompInterface decompiler) throws Exception {
        int instructionCount = 0;
        boolean globalWrites = false;
        boolean globalReads = false;
        boolean usesFs = false;
        boolean usesX87 = false;
        boolean usesSse = false;
        boolean hasLoops = false;
        int unresolvedCalls = 0;
        Set<String> returns = new LinkedHashSet<>();
        Set<String> internal = new LinkedHashSet<>();
        Set<String> external = new LinkedHashSet<>();
        Set<String> strings = new LinkedHashSet<>();
        Map<String, Integer> mnemonics = new TreeMap<>();

        InstructionIterator iterator =
            currentProgram.getListing().getInstructions(function.getBody(), true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = iterator.next();
            instructionCount++;
            String mnemonic = instruction.getMnemonicString().toLowerCase();
            mnemonics.merge(mnemonic, 1, Integer::sum);
            String rendered = instruction.toString().toLowerCase();
            usesFs |= rendered.contains("fs:");
            usesX87 |= isX87(mnemonic, rendered);
            usesSse |= rendered.contains("xmm") || isSseMnemonic(mnemonic);
            if (mnemonic.startsWith("ret")) {
                String suffix = instruction.getNumOperands() == 0
                    ? "" : instruction.getDefaultOperandRepresentation(0);
                returns.add(suffix == null || suffix.isBlank() ? "ret" : "ret " + suffix);
            }

            for (Address flow : instruction.getFlows()) {
                if (function.getBody().contains(flow) &&
                        Long.compareUnsigned(flow.getOffset(), instruction.getAddress().getOffset()) <= 0) {
                    hasLoops = true;
                }
            }

            if (instruction.getFlowType().isCall()) {
                if (!classifyCall(instruction, internal, external)) {
                    unresolvedCalls++;
                }
            }
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.isMemoryReference()) {
                    continue;
                }
                Address target = reference.getToAddress();
                if (reference.getReferenceType().isWrite()) {
                    globalWrites = true;
                }
                if (reference.getReferenceType().isRead()) {
                    globalReads = true;
                }
                if (strings.size() < MAX_STRINGS) {
                    Data data = currentProgram.getListing().getDefinedDataAt(target);
                    if (data != null && data.hasStringValue() && data.getValue() != null) {
                        strings.add(clean(data.getValue().toString(), 120));
                    }
                }
            }
        }

        int callers = directCallers(function);
        int parameters = function.getParameterCount();
        String convention = nullToEmpty(function.getCallingConventionName());
        DecompileResults result =
            decompiler.decompileFunction(function, DECOMPILE_TIMEOUT_SECONDS, monitor);
        if (result.decompileCompleted()) {
            HighFunction high = result.getHighFunction();
            if (high != null && high.getFunctionPrototype() != null) {
                parameters = high.getFunctionPrototype().getNumParams();
                convention = nullToEmpty(high.getFunctionPrototype().getModelName());
            }
        }

        return String.join("\t",
            function.getEntryPoint().toString(),
            Long.toString(function.getBody().getNumAddresses()),
            Integer.toString(instructionCount),
            joinSorted(returns),
            Integer.toString(internal.size()),
            joinSorted(internal),
            joinSorted(external),
            Integer.toString(unresolvedCalls),
            Boolean.toString(globalWrites),
            Boolean.toString(globalReads),
            Boolean.toString(usesFs),
            Boolean.toString(usesX87),
            Boolean.toString(usesSse),
            Boolean.toString(hasLoops),
            Integer.toString(callers),
            String.join(" | ", strings),
            Integer.toString(parameters),
            convention,
            operationProfile(mnemonics));
    }

    private boolean classifyCall(
            Instruction instruction, Set<String> internal, Set<String> external) {
        Set<Address> targets = new LinkedHashSet<>();
        boolean resolvedAny = false;
        Collections.addAll(targets, instruction.getFlows());
        for (Reference reference : instruction.getReferencesFrom()) {
            if (reference.getReferenceType().isCall() || reference.getReferenceType().isIndirect()) {
                targets.add(reference.getToAddress());
            }
        }
        for (Address target : targets) {
            Function called = getFunctionAt(target);
            if (called == null) {
                called = getFunctionContaining(target);
            }
            if (called != null) {
                Function resolved = called;
                Set<Function> seen = new HashSet<>();
                while (resolved.isThunk() && seen.add(resolved) && resolved.getThunkedFunction(false) != null) {
                    resolved = resolved.getThunkedFunction(false);
                }
                if (resolved.isExternal()) {
                    external.add(resolved.getName(true));
                } else {
                    MemoryBlock block = getMemoryBlock(resolved.getEntryPoint());
                    if (block != null && ".text".equals(block.getName())) {
                        internal.add(resolved.getEntryPoint().toString());
                    } else {
                        external.add(resolved.getName(true));
                    }
                }
                resolvedAny = true;
                continue;
            }
            Symbol symbol = getSymbolAt(target);
            if (symbol != null) {
                external.add(symbol.getName(true));
                resolvedAny = true;
            }
        }
        return resolvedAny;
    }

    private int directCallers(Function target) {
        Set<Address> callers = new HashSet<>();
        for (Reference reference : getReferencesTo(target.getEntryPoint())) {
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = getFunctionContaining(reference.getFromAddress());
            if (caller != null) {
                callers.add(caller.getEntryPoint());
            }
        }
        return callers.size();
    }

    private Set<Long> readCleanText(Path ledger) throws Exception {
        Set<Long> result = new HashSet<>();
        try (BufferedReader reader = Files.newBufferedReader(ledger, StandardCharsets.UTF_8)) {
            String[] columns = reader.readLine().split("\t", -1);
            int address = indexOf(columns, "address");
            int section = indexOf(columns, "section");
            int securom = indexOf(columns, "securom");
            String line;
            while ((line = reader.readLine()) != null) {
                String[] values = line.split("\t", -1);
                if (".text".equals(values[section]) && "clean".equals(values[securom])) {
                    result.add(Long.parseUnsignedLong(values[address], 16));
                }
            }
        }
        return result;
    }

    private int indexOf(String[] columns, String wanted) {
        for (int i = 0; i < columns.length; i++) {
            if (wanted.equals(columns[i])) {
                return i;
            }
        }
        throw new IllegalArgumentException("missing ledger column " + wanted);
    }

    private boolean isX87(String mnemonic, String rendered) {
        if (rendered.contains("st0") || rendered.contains("st(")) {
            return true;
        }
        return mnemonic.matches(
            "f(add|sub|subr|mul|div|divr|ld|st|stp|ild|ist|istp|com|comp|compp|ucom|ucomp|" +
            "ucompp|sqrt|abs|chs|sin|cos|sincos|ptan|patan|yl2x|yl2xp1|scale|prem|prem1|rndint|xtract).*");
    }

    private boolean isSseMnemonic(String mnemonic) {
        return mnemonic.matches(
            "(add|sub|mul|div|min|max|sqrt|rsqrt|rcp|cmp|comi|ucomi|cvt|mov|and|or|xor|shuf|unpck)" +
            "(ss|sd|ps|pd).*");
    }

    private String operationProfile(Map<String, Integer> mnemonics) {
        List<String> values = new ArrayList<>();
        for (Map.Entry<String, Integer> entry : mnemonics.entrySet()) {
            values.add(entry.getKey() + ":" + entry.getValue());
        }
        return String.join(",", values);
    }

    private String joinSorted(Set<String> values) {
        List<String> sorted = new ArrayList<>(values);
        Collections.sort(sorted);
        return String.join(",", sorted);
    }

    private String clean(String value, int limit) {
        String clean = value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
        return clean.length() <= limit ? clean : clean.substring(0, limit) + "...";
    }

    private String nullToEmpty(String value) {
        return value == null ? "" : value;
    }
}
