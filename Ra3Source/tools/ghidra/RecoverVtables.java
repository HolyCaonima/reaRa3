// Finds C++ vtable candidates in read-only data and recovers the virtual
// methods they point at. The game was built without RTTI, so Ghidra does not
// follow vtables; many virtual methods are only reachable this way and are
// never turned into functions by auto-analysis.
//
// A vtable candidate is a run of >= kMinSlots consecutive aligned pointers into
// executable, non-SecuROM code. For each slot target that is not already a
// function, the script disassembles and creates one (in memory only; nothing is
// persisted unless run_headless.ps1 is given -Save).
//
// Usage (headless): -postScript RecoverVtables.java <output dir>
//   <output dir>/vtables.tsv        one row per candidate vtable
//   <output dir>/vtable_methods.tsv one row per recovered/known method slot
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class RecoverVtables extends GhidraScript {

    private static final int MIN_SLOTS = 3;
    private static final java.util.Set<String> SECUROM_BLOCKS =
            java.util.Set.of("Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom");

    private int created = 0;
    private int alreadyFunctions = 0;
    private int slotsTotal = 0;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: RecoverVtables.java <output dir>");
        }
        Path outDir = Paths.get(args[0]);
        Files.createDirectories(outDir);

        long base = currentProgram.getImageBase().getOffset();
        long maxAddr = currentProgram.getMaxAddress().getOffset();
        int ptrSize = currentProgram.getDefaultPointerSize();

        try (PrintWriter vt = writer(outDir.resolve("vtables.tsv"));
             PrintWriter methods = writer(outDir.resolve("vtable_methods.tsv"))) {
            vt.println("vtable\tblock\tslots\tnew_functions");
            methods.println("vtable\tindex\tslot_addr\ttarget\tblock\tstate\tname");

            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isInitialized() || block.isExecute() || !block.isRead()) {
                    continue;  // vtables live in read-only data, not code
                }
                if (SECUROM_BLOCKS.contains(block.getName())) {
                    continue;
                }
                Address addr = block.getStart();
                Address end = block.getEnd();
                while (addr.compareTo(end) < 0) {
                    monitor.checkCancelled();
                    java.util.List<Address> targets = collectRun(addr, end, ptrSize, base, maxAddr);
                    if (targets.size() >= MIN_SLOTS) {
                        int newHere = emit(vt, methods, block, addr, targets, ptrSize);
                        vt.println(addr + "\t" + block.getName() + "\t" + targets.size() + "\t" + newHere);
                        addr = addr.add((long) targets.size() * ptrSize);
                    } else {
                        addr = addr.add(ptrSize);
                    }
                }
            }
        }
        println("vtable methods: " + slotsTotal + " slots, " + alreadyFunctions + " already functions, "
                + created + " newly created");
        println("Wrote " + outDir.resolve("vtables.tsv") + " and vtable_methods.tsv");
    }

    // Longest run of consecutive aligned pointers into executable non-SecuROM code.
    private java.util.List<Address> collectRun(Address start, Address blockEnd, int ptrSize, long base, long maxAddr)
            throws Exception {
        java.util.List<Address> targets = new java.util.ArrayList<>();
        Address at = start;
        while (at.compareTo(blockEnd) < 0) {
            long value;
            try {
                value = ptrSize == 8 ? getLong(at) & 0xFFFFFFFFFFFFFFFFL : getInt(at) & 0xFFFFFFFFL;
            } catch (Exception e) {
                break;
            }
            if (value < base || value > maxAddr) {
                break;
            }
            Address target = toAddr(value);
            MemoryBlock targetBlock = getMemoryBlock(target);
            if (targetBlock == null || !targetBlock.isExecute() || SECUROM_BLOCKS.contains(targetBlock.getName())) {
                break;
            }
            targets.add(target);
            at = at.add(ptrSize);
        }
        return targets;
    }

    private int emit(PrintWriter vt, PrintWriter methods, MemoryBlock block, Address vtable,
                     java.util.List<Address> targets, int ptrSize) throws Exception {
        int newHere = 0;
        for (int i = 0; i < targets.size(); i++) {
            Address target = targets.get(i);
            Address slot = vtable.add((long) i * ptrSize);
            slotsTotal++;
            Function existing = getFunctionAt(target);
            String state;
            String name;
            if (existing != null) {
                alreadyFunctions++;
                state = "known";
                name = existing.getName();
            } else {
                Function made = recover(target);
                if (made != null) {
                    created++;
                    newHere++;
                    state = "created";
                    name = made.getName();
                } else {
                    state = "unrecovered";
                    name = "";
                }
            }
            MemoryBlock tb = getMemoryBlock(target);
            methods.println(vtable + "\t" + i + "\t" + slot + "\t" + target + "\t"
                    + (tb == null ? "?" : tb.getName()) + "\t" + state + "\t" + name);
        }
        return newHere;
    }

    private Function recover(Address target) {
        if (getInstructionAt(target) == null) {
            try {
                disassemble(target);
            } catch (Exception e) {
                return null;
            }
        }
        Instruction first = getInstructionAt(target);
        if (first == null) {
            return null;
        }
        return createFunction(target, null);
    }

    private PrintWriter writer(Path path) throws Exception {
        return new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8));
    }
}
