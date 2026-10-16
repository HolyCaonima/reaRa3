// For every indirect JMP from original code into a SecuROM block, checks whether the bytes after
// the JMP still hold a linear run of valid code (i.e. only the leading instruction was replaced).
// Disassembly happens in memory only; nothing is saved unless run_headless.ps1 is given -Save.
// Usage (headless): -postScript ProbeSecuromThunkBodies.java <output tsv>
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.FlowType;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class ProbeSecuromThunkBodies extends GhidraScript {

    private static final Set<String> SECUROM_BLOCKS =
            Set.of("Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom");
    private static final int MAX_LINEAR_INSTRUCTIONS = 4000;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: ProbeSecuromThunkBodies.java <output tsv>");
        }
        Path out = Paths.get(args[0]);
        Files.createDirectories(out.toAbsolutePath().getParent());

        Map<String, Integer> totals = new TreeMap<>();
        try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out, StandardCharsets.UTF_8))) {
            w.println("site\tfunction\tat_entry\tfollowing\tclass\tlinear_instructions\tend_instruction");
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isExecute() || SECUROM_BLOCKS.contains(block.getName())) {
                    continue;
                }
                Instruction insn = getInstructionAt(block.getStart());
                if (insn == null) {
                    insn = getInstructionAfter(block.getStart());
                }
                for (; insn != null && block.contains(insn.getAddress()); insn = getInstructionAfter(insn.getAddress())) {
                    monitor.checkCancelled();
                    if (!isJumpIntoSecurom(insn)) {
                        continue;
                    }
                    Function f = getFunctionContaining(insn.getAddress());
                    boolean atEntry = f != null && f.getEntryPoint().equals(insn.getAddress());
                    Address following = insn.getMaxAddress().next();
                    String[] probe = probe(following);
                    totals.merge((atEntry ? "entry:" : "inner:") + probe[0], 1, Integer::sum);
                    w.println(insn.getAddress() + "\t" + (f == null ? "<none>" : f.getName()) + "\t" + atEntry
                            + "\t" + following + "\t" + probe[0] + "\t" + probe[1] + "\t" + probe[2]);
                }
            }
        }
        for (Map.Entry<String, Integer> e : totals.entrySet()) {
            println(e.getKey() + " = " + e.getValue());
        }
        println("Wrote " + out.toAbsolutePath());
    }

    private boolean isJumpIntoSecurom(Instruction insn) {
        if (!insn.getFlowType().isJump()) {
            return false;
        }
        for (Reference ref : insn.getReferencesFrom()) {
            MemoryBlock target = getMemoryBlock(ref.getToAddress());
            if (target != null && SECUROM_BLOCKS.contains(target.getName())) {
                return true;
            }
        }
        return false;
    }

    // Returns {class, linear instruction count, last instruction}.
    private String[] probe(Address start) throws Exception {
        MemoryBlock block = getMemoryBlock(start);
        if (block == null || !block.isInitialized()) {
            return new String[] {"outside-image", "0", ""};
        }
        byte first = getByte(start);
        if (first == (byte) 0xCC || first == (byte) 0x90) {
            return new String[] {"padding", "0", ""};
        }
        if (getInstructionAt(start) == null && getDataAt(start) == null) {
            disassemble(start);
        }
        Address at = start;
        for (int count = 1; count <= MAX_LINEAR_INSTRUCTIONS; count++) {
            Instruction insn = getInstructionAt(at);
            if (insn == null) {
                return new String[] {count == 1 ? "undecodable" : "breaks", String.valueOf(count - 1), ""};
            }
            FlowType flow = insn.getFlowType();
            String mnemonic = insn.getMnemonicString();
            if (mnemonic.startsWith("RET")) {
                return new String[] {"reaches-ret", String.valueOf(count), insn.toString()};
            }
            if (mnemonic.equals("INT3") || mnemonic.equals("HLT")) {
                return new String[] {"hits-" + mnemonic.toLowerCase(), String.valueOf(count), insn.toString()};
            }
            if (flow.isJump() && flow.isUnConditional()) {
                return new String[] {"ends-in-jmp", String.valueOf(count), insn.toString()};
            }
            at = insn.getMaxAddress().next();
        }
        return new String[] {"too-long", String.valueOf(MAX_LINEAR_INSTRUCTIONS), ""};
    }
}
