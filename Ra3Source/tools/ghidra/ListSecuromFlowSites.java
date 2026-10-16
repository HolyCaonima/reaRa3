// Lists every control-flow reference from original code blocks into the
// SecuROM-added blocks, with the instruction at the site and at the target.
// Usage (headless): -postScript ListSecuromFlowSites.java <output .tsv>
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Set;

public class ListSecuromFlowSites extends GhidraScript {

    private static final Set<String> SECUROM_BLOCKS =
        Set.of("Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom");

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args.length > 0 ? args[0] : "securom_flow_sites.tsv");
        Files.createDirectories(out.toAbsolutePath().getParent());
        Memory memory = currentProgram.getMemory();

        try (BufferedWriter w = Files.newBufferedWriter(out, StandardCharsets.UTF_8)) {
            w.write("site\tfunction\tfunction_entry\tsite_instruction\tref_type\ttarget\ttarget_block\ttarget_instruction\tnext_after_site\n");
            for (MemoryBlock block : memory.getBlocks()) {
                if (!block.isExecute() || SECUROM_BLOCKS.contains(block.getName())) continue;
                AddressIterator sources = currentProgram.getReferenceManager()
                    .getReferenceSourceIterator(new AddressSet(block.getStart(), block.getEnd()), true);
                while (sources.hasNext()) {
                    monitor.checkCancelled();
                    Address from = sources.next();
                    for (Reference ref : getReferencesFrom(from)) {
                        if (!ref.getReferenceType().isFlow()) continue;
                        MemoryBlock target = memory.getBlock(ref.getToAddress());
                        if (target == null || !SECUROM_BLOCKS.contains(target.getName())) continue;
                        Function f = getFunctionContaining(from);
                        Instruction site = getInstructionAt(from);
                        Instruction dest = getInstructionAt(ref.getToAddress());
                        Instruction next = site == null ? null : site.getNext();
                        w.write(from + "\t" + (f == null ? "" : f.getName(true)) + "\t" +
                                (f == null ? "" : f.getEntryPoint().toString()) + "\t" +
                                (site == null ? "" : site.toString()) + "\t" + ref.getReferenceType() + "\t" +
                                ref.getToAddress() + "\t" + target.getName() + "\t" +
                                (dest == null ? "<no instruction>" : dest.toString()) + "\t" +
                                (next == null ? "" : next.getAddress() + " " + next) + "\n");
                    }
                }
            }
        }
        println("SecuROM flow sites written to " + out.toAbsolutePath());
    }
}
