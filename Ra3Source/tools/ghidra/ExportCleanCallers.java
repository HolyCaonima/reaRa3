// Lists direct callers whose ledger SecuROM status is clean.
// Usage: ExportCleanCallers.java <output.tsv> <ledger.tsv> <target>...
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class ExportCleanCallers extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException(
                    "usage: ExportCleanCallers.java <output.tsv> <ledger.tsv> <target>...");
        }
        Map<Long, String> ledger = readLedger(Paths.get(args[1]));
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.toAbsolutePath().getParent());
        try (PrintWriter out = new PrintWriter(
                Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
            out.println("target\tcaller\tsite");
            for (int i = 2; i < args.length; i++) {
                for (Reference reference : getReferencesTo(toAddr(args[i]))) {
                    if (!reference.getReferenceType().isCall()) {
                        continue;
                    }
                    Function caller = getFunctionContaining(reference.getFromAddress());
                    if (caller != null &&
                            "clean".equals(ledger.get(caller.getEntryPoint().getOffset()))) {
                        out.printf("%s\t%s\t%s%n", args[i], caller.getEntryPoint(),
                                reference.getFromAddress());
                    }
                }
            }
        }
    }

    private Map<Long, String> readLedger(Path path) throws Exception {
        Map<Long, String> result = new HashMap<>();
        try (BufferedReader reader = Files.newBufferedReader(path, StandardCharsets.UTF_8)) {
            String[] columns = reader.readLine().split("\t", -1);
            int address = indexOf(columns, "address");
            int securom = indexOf(columns, "securom");
            String line;
            while ((line = reader.readLine()) != null) {
                String[] values = line.split("\t", -1);
                result.put(Long.parseUnsignedLong(values[address], 16), values[securom]);
            }
        }
        return result;
    }

    private int indexOf(String[] values, String wanted) {
        for (int i = 0; i < values.length; i++) {
            if (wanted.equals(values[i])) {
                return i;
            }
        }
        throw new IllegalArgumentException("missing ledger column " + wanted);
    }
}
