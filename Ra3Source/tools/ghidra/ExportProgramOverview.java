// Exports an overview of the analysed program for Ra3Source/docs/analysis.
// Usage (headless): -postScript ExportProgramOverview.java <output directory>
//
// Writes:
//   ghidra_overview.json  memory blocks with function/instruction/string counts,
//                         function totals, entry point, flow references into
//                         SecuROM-added blocks
//   source_file_functions.tsv  functions referencing each embedded source path
//   functions.tsv              every function: entry, size, block, name
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.regex.Pattern;

public class ExportProgramOverview extends GhidraScript {

    private static final Set<String> SECUROM_BLOCKS =
        Set.of("Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom");
    private static final Pattern SOURCE_PATH =
        Pattern.compile("(?i)^.*\\\\ra3\\\\code\\\\(.+\\.(cpp|h|hpp|inl))$");

    private static final class BlockStats {
        int functions;
        long instructions;
        int strings;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args.length > 0 ? args[0] : ".");
        Files.createDirectories(out);

        Memory memory = currentProgram.getMemory();
        Listing listing = currentProgram.getListing();
        FunctionManager functions = currentProgram.getFunctionManager();
        ReferenceManager references = currentProgram.getReferenceManager();

        Map<String, BlockStats> blocks = new LinkedHashMap<>();
        for (MemoryBlock block : memory.getBlocks()) {
            blocks.put(block.getName(), new BlockStats());
        }

        int total = 0, thunks = 0, named = 0;
        try (BufferedWriter w = Files.newBufferedWriter(out.resolve("functions.tsv"), StandardCharsets.UTF_8)) {
            w.write("entry\tsize\tblock\tsource\tname\n");
            for (Function f : functions.getFunctions(true)) {
                monitor.checkCancelled();
                total++;
                if (f.isThunk()) thunks++;
                SourceType source = f.getSymbol().getSource();
                if (source != SourceType.DEFAULT) named++;
                MemoryBlock block = memory.getBlock(f.getEntryPoint());
                String blockName = block == null ? "?" : block.getName();
                if (block != null) blocks.get(blockName).functions++;
                w.write(f.getEntryPoint() + "\t" + f.getBody().getNumAddresses() + "\t" + blockName + "\t" +
                        source + "\t" + f.getName(true) + "\n");
            }
        }

        for (MemoryBlock block : memory.getBlocks()) {
            if (!block.isExecute() || !block.isInitialized()) continue;
            long count = 0;
            InstructionIterator it = listing.getInstructions(new AddressSet(block.getStart(), block.getEnd()), true);
            while (it.hasNext()) {
                monitor.checkCancelled();
                it.next();
                count++;
            }
            blocks.get(block.getName()).instructions = count;
        }

        Map<String, List<String>> sourceFunctions = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        int stringCount = 0;
        for (Data data : listing.getDefinedData(true)) {
            monitor.checkCancelled();
            if (!data.hasStringValue()) continue;
            stringCount++;
            MemoryBlock block = memory.getBlock(data.getAddress());
            if (block != null) blocks.get(block.getName()).strings++;
            Object value = data.getValue();
            if (value == null) continue;
            var m = SOURCE_PATH.matcher(value.toString());
            if (!m.matches()) continue;
            List<String> users = sourceFunctions.computeIfAbsent(m.group(1).replace('/', '\\'), k -> new ArrayList<>());
            for (Reference ref : references.getReferencesTo(data.getAddress())) {
                Function f = functions.getFunctionContaining(ref.getFromAddress());
                MemoryBlock fromBlock = memory.getBlock(ref.getFromAddress());
                String user = (f == null ? "<no function>@" + ref.getFromAddress() : f.getName(true) + "@" + f.getEntryPoint())
                    + "[" + (fromBlock == null ? "?" : fromBlock.getName()) + "]";
                if (!users.contains(user)) users.add(user);
            }
        }
        try (BufferedWriter w = Files.newBufferedWriter(out.resolve("source_file_functions.tsv"), StandardCharsets.UTF_8)) {
            w.write("source_path\tfunction_count\tfunctions\n");
            for (Map.Entry<String, List<String>> e : sourceFunctions.entrySet()) {
                w.write(e.getKey() + "\t" + e.getValue().size() + "\t" + String.join(" ", e.getValue()) + "\n");
            }
        }

        // Flow references (calls/jumps) from original code blocks into SecuROM-added blocks.
        Map<String, int[]> flowIntoProtected = new TreeMap<>();
        Set<Address> protectedTargets = new HashSet<>();
        Set<Address> callersOfProtected = new HashSet<>();
        for (MemoryBlock block : memory.getBlocks()) {
            if (!block.isExecute() || SECUROM_BLOCKS.contains(block.getName())) continue;
            AddressIterator sources = references.getReferenceSourceIterator(new AddressSet(block.getStart(), block.getEnd()), true);
            while (sources.hasNext()) {
                monitor.checkCancelled();
                Address from = sources.next();
                for (Reference ref : references.getReferencesFrom(from)) {
                    if (!ref.getReferenceType().isFlow()) continue;
                    MemoryBlock target = memory.getBlock(ref.getToAddress());
                    if (target == null || !SECUROM_BLOCKS.contains(target.getName())) continue;
                    int[] counts = flowIntoProtected.computeIfAbsent(block.getName() + "->" + target.getName(), k -> new int[2]);
                    counts[0]++;
                    if (ref.getReferenceType().isCall()) counts[1]++;
                    protectedTargets.add(ref.getToAddress());
                    Function caller = functions.getFunctionContaining(from);
                    if (caller != null) callersOfProtected.add(caller.getEntryPoint());
                }
            }
        }

        StringBuilder json = new StringBuilder();
        json.append("{\n");
        json.append("  \"program\": ").append(quote(currentProgram.getName())).append(",\n");
        json.append("  \"language\": ").append(quote(currentProgram.getLanguageID().toString())).append(",\n");
        json.append("  \"compiler_spec\": ").append(quote(currentProgram.getCompilerSpec().getCompilerSpecID().toString())).append(",\n");
        json.append("  \"image_base\": ").append(quote(currentProgram.getImageBase().toString())).append(",\n");
        json.append("  \"functions\": {\"total\": ").append(total).append(", \"thunks\": ").append(thunks)
            .append(", \"non_default_names\": ").append(named).append("},\n");
        json.append("  \"defined_strings\": ").append(stringCount).append(",\n");
        json.append("  \"source_paths_with_strings\": ").append(sourceFunctions.size()).append(",\n");
        AddressIterator entries = currentProgram.getSymbolTable().getExternalEntryPointIterator();
        int entryCount = 0;
        List<String> securomEntries = new ArrayList<>();
        while (entries.hasNext()) {
            Address a = entries.next();
            entryCount++;
            MemoryBlock b = memory.getBlock(a);
            if (b != null && SECUROM_BLOCKS.contains(b.getName())) securomEntries.add(quote(a + " " + b.getName()));
        }
        json.append("  \"external_entry_points\": ").append(entryCount).append(",\n");
        json.append("  \"external_entry_points_in_securom_blocks\": [").append(String.join(", ", securomEntries)).append("],\n");
        json.append("  \"flow_into_securom_blocks\": {");
        boolean first = true;
        for (Map.Entry<String, int[]> e : flowIntoProtected.entrySet()) {
            json.append(first ? "" : ", ").append(quote(e.getKey())).append(": {\"flow\": ").append(e.getValue()[0])
                .append(", \"calls\": ").append(e.getValue()[1]).append("}");
            first = false;
        }
        json.append("},\n");
        json.append("  \"distinct_securom_targets\": ").append(protectedTargets.size()).append(",\n");
        json.append("  \"functions_calling_into_securom_blocks\": ").append(callersOfProtected.size()).append(",\n");
        json.append("  \"blocks\": [\n");
        first = true;
        for (MemoryBlock block : memory.getBlocks()) {
            BlockStats s = blocks.get(block.getName());
            json.append(first ? "" : ",\n").append("    {\"name\": ").append(quote(block.getName()))
                .append(", \"start\": ").append(quote(block.getStart().toString()))
                .append(", \"size\": ").append(block.getSize())
                .append(", \"perm\": ").append(quote((block.isRead() ? "r" : "-") + (block.isWrite() ? "w" : "-") + (block.isExecute() ? "x" : "-")))
                .append(", \"initialized\": ").append(block.isInitialized())
                .append(", \"functions\": ").append(s.functions)
                .append(", \"instructions\": ").append(s.instructions)
                .append(", \"strings\": ").append(s.strings).append("}");
            first = false;
        }
        json.append("\n  ]\n}\n");
        Files.writeString(out.resolve("ghidra_overview.json"), json.toString(), StandardCharsets.UTF_8);
        println("Ra3Source overview written to " + out.toAbsolutePath());
    }

    private static String quote(String s) {
        StringBuilder b = new StringBuilder("\"");
        for (char c : s.toCharArray()) {
            switch (c) {
                case '"': b.append("\\\""); break;
                case '\\': b.append("\\\\"); break;
                case '\n': b.append("\\n"); break;
                case '\r': b.append("\\r"); break;
                case '\t': b.append("\\t"); break;
                default:
                    if (c < 0x20) b.append(String.format("\\u%04x", (int) c));
                    else b.append(c);
            }
        }
        return b.append('"').toString();
    }
}
