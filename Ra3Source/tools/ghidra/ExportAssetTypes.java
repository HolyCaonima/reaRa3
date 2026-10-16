// Exports compiled-asset handler registrations and the known type-factory leaf.
//
// Usage:
//   ExportAssetTypes.java <program.tsv> <factory.tsv> <function_ledger.tsv>
//
// The ledger is mandatory: callers whose SecuROM status is not "clean" are
// rejected rather than inspected.  The factory entry is the project-specific
// missing entry point documented at 0x004B1C80.
//@category Ra3Source

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class ExportAssetTypes extends GhidraScript {
    private static final long REGISTER_HANDLER = 0x00417220L;
    private static final long FACTORY_ENTRY = 0x004B1C80L;
    private static final long ALLOCATOR = 0x00416820L;

    private Map<Long, String> securom;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                    "usage: ExportAssetTypes.java <program.tsv> <factory.tsv> <function_ledger.tsv>");
        }
        securom = readLedger(Paths.get(args[2]));
        exportRegistrars(Paths.get(args[0]));
        exportFactory(Paths.get(args[1]));
    }

    private void exportRegistrars(Path output) throws Exception {
        Files.createDirectories(output.toAbsolutePath().getParent());
        int emitted = 0;
        try (PrintWriter out = writer(output)) {
            out.println("typeId\ttypeHash\thandler_vtable\tregistrar_address\tdescriptor_address\ttypeHash_source");
            for (Reference reference : getReferencesTo(toAddr(REGISTER_HANDLER))) {
                monitor.checkCancelled();
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Instruction call = getInstructionAt(reference.getFromAddress());
                Function caller = getFunctionContaining(reference.getFromAddress());
                if (call == null || caller == null) {
                    println("Skipping registration call without instruction/function at " +
                            reference.getFromAddress());
                    continue;
                }
                requireClean(caller);
                Address descriptor = findPushedAddress(call, 12);
                if (descriptor == null) {
                    DynamicRegistration dynamic = recoverDynamicRegistration(caller, call);
                    if (dynamic != null) {
                        out.printf("%08X\t%08X\t%08X\t%s\tdynamic\t%s%n",
                                dynamic.typeId, dynamic.typeHash, dynamic.vtable,
                                caller.getEntryPoint(), dynamic.hashSource);
                        emitted++;
                        continue;
                    }
                    println("Could not recover dynamic descriptor for registrar " +
                            caller.getEntryPoint());
                    continue;
                }
                long vtable = readU32(descriptor);
                long typeId = readU32(descriptor.add(4));
                HashValue hash = recoverAssignedHash(caller, call, descriptor.add(8));
                if (hash == null) {
                    long initialized = readU32(descriptor.add(8));
                    hash = new HashValue(initialized, initialized == 0 ? "" : descriptor.add(8).toString());
                }
                out.printf("%08X\t%08X\t%08X\t%s\t%s\t%s%n",
                        typeId, hash.value, vtable, caller.getEntryPoint(), descriptor,
                        hash.source);
                emitted++;
            }
        }
        println("Exported " + emitted + " asset handler registrations to " + output);
    }

    private DynamicRegistration recoverDynamicRegistration(Function caller, Instruction call)
            throws Exception {
        Long typeId = null;
        Long vtable = null;
        Long typeHash = null;
        String hashSource = "";
        Instruction instruction = call;
        for (int i = 0; i < 24; i++) {
            instruction = getInstructionBefore(instruction.getAddress());
            if (instruction == null || !caller.getBody().contains(instruction.getAddress())) {
                break;
            }
            for (Reference reference : instruction.getReferencesFrom()) {
                Address target = reference.getToAddress();
                if (target == null || getMemoryBlock(target) == null ||
                        getMemoryBlock(target).isExecute()) {
                    continue;
                }
                if (reference.getReferenceType().isRead()) {
                    typeHash = readU32(target);
                    hashSource = target.toString();
                } else if (looksLikeVtable(target)) {
                    vtable = target.getOffset();
                }
            }
            for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                Long value = scalarOperand(instruction, operand);
                if (value != null && value > 0xFFFF && getMemoryBlock(toAddr(value)) == null) {
                    typeId = value;
                }
                for (Object object : instruction.getOpObjects(operand)) {
                    if (object instanceof Address) {
                        Address address = (Address) object;
                        if (looksLikeVtable(address)) {
                            vtable = address.getOffset();
                        }
                    }
                }
            }
        }
        if (typeId == null || vtable == null || typeHash == null) {
            return null;
        }
        return new DynamicRegistration(typeId, typeHash, vtable, hashSource);
    }

    private boolean looksLikeVtable(Address address) {
        try {
            long first = readU32(address);
            Address target = toAddr(first);
            return getMemoryBlock(target) != null && getMemoryBlock(target).isExecute() &&
                    ".text".equals(getMemoryBlock(target).getName());
        } catch (Exception exception) {
            return false;
        }
    }

    private HashValue recoverAssignedHash(Function caller, Instruction call, Address field)
            throws Exception {
        Instruction instruction = call;
        for (int i = 0; i < 16; i++) {
            instruction = getInstructionBefore(instruction.getAddress());
            if (instruction == null || !caller.getBody().contains(instruction.getAddress())) {
                break;
            }
            boolean writesField = false;
            for (Reference reference : instruction.getReferencesFrom()) {
                if (reference.getToAddress().equals(field) && reference.getReferenceType().isWrite()) {
                    writesField = true;
                }
            }
            if (!writesField) {
                continue;
            }
            // Most initializers are MOV [descriptor+8],EAX immediately after
            // MOV EAX,[.rdata constant].  Handle an immediate source too.
            Long immediate = scalarOperand(instruction, 1);
            if (immediate != null && immediate > 0xFFFF) {
                return new HashValue(immediate, "immediate");
            }
            Instruction producer = instruction;
            for (int back = 0; back < 4; back++) {
                producer = getInstructionBefore(producer.getAddress());
                if (producer == null || !caller.getBody().contains(producer.getAddress())) {
                    break;
                }
                for (Reference reference : producer.getReferencesFrom()) {
                    Address source = reference.getToAddress();
                    if (reference.getReferenceType().isRead() && source != null &&
                            getMemoryBlock(source) != null && !getMemoryBlock(source).isExecute()) {
                        return new HashValue(readU32(source), source.toString());
                    }
                }
            }
        }
        return null;
    }

    private void exportFactory(Path output) throws Exception {
        Address entry = toAddr(FACTORY_ENTRY);
        Function function = getFunctionAt(entry);
        if (function == null) {
            disassemble(entry);
            function = createFunction(entry, null);
        }
        if (function == null) {
            throw new IllegalStateException("could not create factory function at " + entry);
        }
        int emitted = 0;
        try (PrintWriter out = writer(output)) {
            out.println("typeId\talloc_size\tcase_address\tfactory_address");
            Instruction instruction = getInstructionAt(entry);
            while (instruction != null && function.getBody().contains(instruction.getAddress())) {
                monitor.checkCancelled();
                if ("CMP".equals(instruction.getMnemonicString())) {
                    Long typeId = largestScalar(instruction);
                    if (typeId != null && typeId > 0xFFFF) {
                        AllocSite allocation = findAllocation(function, instruction, 80);
                        if (allocation != null) {
                            out.printf("%08X\t%d\t%s\t%s%n", typeId, allocation.size,
                                    instruction.getAddress(), entry);
                            emitted++;
                        }
                    }
                }
                instruction = getInstructionAfter(instruction.getAddress());
            }
        }
        println("Exported " + emitted + " factory cases to " + output +
                " (body " + function.getBody() + ")");
    }

    private AllocSite findAllocation(Function function, Instruction comparison, int limit) {
        ArrayDeque<PathState> pending = new ArrayDeque<>();
        Address first = comparison.getFallThrough();
        if (first != null) {
            pending.add(new PathState(first, null, 0));
        }
        Set<String> visited = new HashSet<>();
        while (!pending.isEmpty()) {
            PathState state = pending.removeFirst();
            if (state.depth >= limit || !function.getBody().contains(state.address)) {
                continue;
            }
            String key = state.address + ":" + state.lastPush;
            if (!visited.add(key)) {
                continue;
            }
            Instruction instruction = getInstructionAt(state.address);
            if (instruction == null || "CMP".equals(instruction.getMnemonicString())) {
                continue;
            }
            Long lastPush = state.lastPush;
            if ("PUSH".equals(instruction.getMnemonicString())) {
                Long value = scalarOperand(instruction, 0);
                if (value != null && value > 0 && value <= 0x100000) {
                    lastPush = value;
                }
            } else if ("CALL".equals(instruction.getMnemonicString())) {
                for (Reference reference : instruction.getReferencesFrom()) {
                    if (reference.getReferenceType().isCall() &&
                            reference.getToAddress().getOffset() == ALLOCATOR &&
                            lastPush != null) {
                        return new AllocSite(lastPush);
                    }
                }
            }
            Address fallThrough = instruction.getFallThrough();
            if (fallThrough != null) {
                pending.add(new PathState(fallThrough, lastPush, state.depth + 1));
            }
            for (Address flow : instruction.getFlows()) {
                if (function.getBody().contains(flow)) {
                    pending.add(new PathState(flow, lastPush, state.depth + 1));
                }
            }
        }
        return null;
    }

    private Address findPushedAddress(Instruction call, int limit) {
        Instruction instruction = call;
        for (int i = 0; i < limit; i++) {
            instruction = getInstructionBefore(instruction.getAddress());
            if (instruction == null) {
                break;
            }
            if ("PUSH".equals(instruction.getMnemonicString())) {
                for (Object object : instruction.getOpObjects(0)) {
                    if (object instanceof Address) {
                        return (Address) object;
                    }
                    if (object instanceof Scalar) {
                        long value = ((Scalar) object).getUnsignedValue();
                        Address address = toAddr(value);
                        if (getMemoryBlock(address) != null) {
                            return address;
                        }
                    }
                }
            }
        }
        return null;
    }

    private Long largestScalar(Instruction instruction) {
        Long result = null;
        for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
            Long value = scalarOperand(instruction, operand);
            if (value != null && (result == null || value > result)) {
                result = value;
            }
        }
        return result;
    }

    private Long scalarOperand(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) {
            return null;
        }
        for (Object object : instruction.getOpObjects(operand)) {
            if (object instanceof Scalar) {
                return ((Scalar) object).getUnsignedValue();
            }
        }
        return null;
    }

    private long readU32(Address address) throws Exception {
        return getInt(address) & 0xFFFFFFFFL;
    }

    private void requireClean(Function function) {
        long address = function.getEntryPoint().getOffset();
        String status = securom.get(address);
        if (!"clean".equals(status)) {
            throw new IllegalStateException("refusing to inspect non-clean/unlisted function " +
                    function.getEntryPoint() + " (ledger status " + status + ")");
        }
    }

    private Map<Long, String> readLedger(Path path) throws Exception {
        Map<Long, String> result = new HashMap<>();
        try (BufferedReader reader = Files.newBufferedReader(path, StandardCharsets.UTF_8)) {
            String header = reader.readLine();
            if (header == null) {
                throw new IllegalArgumentException("empty ledger");
            }
            String[] columns = header.split("\t", -1);
            int addressColumn = indexOf(columns, "address");
            int securomColumn = indexOf(columns, "securom");
            String line;
            while ((line = reader.readLine()) != null) {
                String[] values = line.split("\t", -1);
                result.put(Long.parseUnsignedLong(values[addressColumn], 16),
                        values[securomColumn]);
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

    private PrintWriter writer(Path path) throws Exception {
        Files.createDirectories(path.toAbsolutePath().getParent());
        return new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8));
    }

    private static class HashValue {
        final long value;
        final String source;
        HashValue(long value, String source) {
            this.value = value;
            this.source = source;
        }
    }

    private static class AllocSite {
        final long size;
        AllocSite(long size) {
            this.size = size;
        }
    }

    private static class PathState {
        final Address address;
        final Long lastPush;
        final int depth;
        PathState(Address address, Long lastPush, int depth) {
            this.address = address;
            this.lastPush = lastPush;
            this.depth = depth;
        }
    }

    private static class DynamicRegistration {
        final long typeId;
        final long typeHash;
        final long vtable;
        final String hashSource;
        DynamicRegistration(long typeId, long typeHash, long vtable, String hashSource) {
            this.typeId = typeId;
            this.typeHash = typeHash;
            this.vtable = vtable;
            this.hashSource = hashSource;
        }
    }
}
