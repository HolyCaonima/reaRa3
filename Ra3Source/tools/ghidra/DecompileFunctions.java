// Decompiles selected functions into one text file.
// Usage (headless): -postScript DecompileFunctions.java <output file> <selector>...
//
// Selectors:
//   0x00401000 / 00401000   function containing that address
//   string:<text>           every function referencing a defined string containing <text>
//   calls:<address>         every function calling the function at <address>
//   entry:<address>         function starting at <address>, created in memory if Ghidra missed it
//                           (only persisted when run_headless.ps1 is given -Save)
//@category Ra3Source

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.LinkedHashMap;
import java.util.Map;

public class DecompileFunctions extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("usage: DecompileFunctions.java <output file> <selector>...");
        }
        Path out = Paths.get(args[0]);
        Files.createDirectories(out.toAbsolutePath().getParent());

        Map<Function, String> selected = new LinkedHashMap<>();
        for (int i = 1; i < args.length; i++) {
            select(args[i], selected);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (BufferedWriter w = Files.newBufferedWriter(out, StandardCharsets.UTF_8)) {
            for (Map.Entry<Function, String> e : selected.entrySet()) {
                monitor.checkCancelled();
                Function f = e.getKey();
                w.write("// ==== " + f.getName(true) + " @ " + f.getEntryPoint() + "  (" + e.getValue() + ")\n");
                DecompileResults result = decompiler.decompileFunction(f, 120, monitor);
                if (result.decompileCompleted()) {
                    w.write(result.getDecompiledFunction().getC());
                } else {
                    w.write("// decompilation failed: " + result.getErrorMessage() + "\n");
                }
                w.write("\n");
            }
        } finally {
            decompiler.dispose();
        }
        println("Decompiled " + selected.size() + " function(s) to " + out.toAbsolutePath());
    }

    private void select(String selector, Map<Function, String> selected) throws Exception {
        if (selector.startsWith("string:")) {
            String needle = selector.substring("string:".length());
            for (Data data : currentProgram.getListing().getDefinedData(true)) {
                monitor.checkCancelled();
                if (!data.hasStringValue() || data.getValue() == null || !data.getValue().toString().contains(needle)) {
                    continue;
                }
                for (Reference ref : getReferencesTo(data.getAddress())) {
                    addContaining(ref.getFromAddress(), selector + " @ " + data.getAddress(), selected);
                }
            }
        } else if (selector.startsWith("calls:")) {
            Address target = toAddr(selector.substring("calls:".length()));
            for (Reference ref : getReferencesTo(target)) {
                if (ref.getReferenceType().isCall()) {
                    addContaining(ref.getFromAddress(), selector, selected);
                }
            }
        } else if (selector.startsWith("entry:")) {
            Address entry = toAddr(selector.substring("entry:".length()));
            Function f = getFunctionAt(entry);
            if (f == null) {
                disassemble(entry);
                f = createFunction(entry, null);
            }
            if (f == null) {
                println("could not create a function at " + entry);
            } else {
                selected.putIfAbsent(f, selector);
            }
        } else {
            addContaining(toAddr(selector), selector, selected);
        }
    }

    private void addContaining(Address address, String why, Map<Function, String> selected) {
        Function f = getFunctionContaining(address);
        if (f == null) {
            println("no function contains " + address + " (" + why + ")");
            return;
        }
        selected.putIfAbsent(f, why);
    }
}
