// Decompiles explicitly selected RA3 functions after checking the function
// ledger. This is an evidence helper; it refuses every non-.text/non-clean
// function rather than crossing the SecuROM boundary.
//
// Usage:
//   DecompileCleanFunctions.java <output.txt> <function_ledger.tsv> <VA>...
//@category Ra3Source

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class DecompileCleanFunctions extends GhidraScript {
    private Map<Long, LedgerEntry> ledger;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException(
                    "usage: DecompileCleanFunctions.java <output.txt> <function_ledger.tsv> <VA>...");
        }
        Path output = Paths.get(args[0]);
        ledger = readLedger(Paths.get(args[1]));
        Files.createDirectories(output.toAbsolutePath().getParent());

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("could not open program in decompiler");
        }
        try (PrintWriter out = new PrintWriter(
                Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
            for (int index = 2; index < args.length; index++) {
                monitor.checkCancelled();
                long value = Long.parseUnsignedLong(
                        args[index].replaceFirst("^(0x|0X)", ""), 16);
                Address address = toAddr(value);
                Function function = getFunctionAt(address);
                LedgerEntry entry = ledger.get(value);
                MemoryBlock block = getMemoryBlock(address);
                if (entry == null || !"clean".equals(entry.securom) ||
                        block == null || !".text".equals(block.getName())) {
                    throw new IllegalStateException(
                            "refusing non-clean/non-.text function " + address);
                }
                if (function == null) {
                    throw new IllegalStateException("no function at " + address);
                }
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted()) {
                    throw new IllegalStateException(
                            "decompilation failed at " + address + ": " + result.getErrorMessage());
                }
                out.println("===== " + address + " " + function.getName() + " =====");
                out.println(result.getDecompiledFunction().getC());
            }
        } finally {
            decompiler.dispose();
        }
        println("Wrote clean decompilations to " + output);
    }

    private Map<Long, LedgerEntry> readLedger(Path path) throws Exception {
        Map<Long, LedgerEntry> result = new HashMap<>();
        try (BufferedReader reader = Files.newBufferedReader(path, StandardCharsets.UTF_8)) {
            String header = reader.readLine();
            if (header == null) {
                throw new IllegalArgumentException("empty ledger");
            }
            String[] columns = header.split("\t", -1);
            int addressColumn = indexOf(columns, "address");
            int sectionColumn = indexOf(columns, "section");
            int securomColumn = indexOf(columns, "securom");
            String line;
            while ((line = reader.readLine()) != null) {
                String[] values = line.split("\t", -1);
                if (".text".equals(values[sectionColumn])) {
                    result.put(
                            Long.parseUnsignedLong(values[addressColumn], 16),
                            new LedgerEntry(values[securomColumn]));
                }
            }
        }
        return result;
    }

    private int indexOf(String[] values, String wanted) {
        for (int index = 0; index < values.length; index++) {
            if (wanted.equals(values[index])) {
                return index;
            }
        }
        throw new IllegalArgumentException("missing ledger column " + wanted);
    }

    private static class LedgerEntry {
        final String securom;

        LedgerEntry(String securom) {
            this.securom = securom;
        }
    }
}
