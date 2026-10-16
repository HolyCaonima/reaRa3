// Exports every code/data reference to selected addresses.
// Usage: ExportAddressReferences.java <output.tsv> <VA>...
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class ExportAddressReferences extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                    "usage: ExportAddressReferences.java <output.tsv> <VA>...");
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.toAbsolutePath().getParent());
        try (PrintWriter out = new PrintWriter(
                Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
            out.println("target\tfrom\ttype\tfunction");
            for (int i = 1; i < args.length; i++) {
                for (Reference reference : getReferencesTo(toAddr(args[i]))) {
                    Function function =
                            getFunctionContaining(reference.getFromAddress());
                    out.printf("%s\t%s\t%s\t%s%n", args[i],
                            reference.getFromAddress(),
                            reference.getReferenceType(),
                            function == null ? "" :
                                    function.getEntryPoint().toString());
                }
            }
        }
    }
}
