// Re-create one function over an explicit address range (dropping functions Ghidra found inside it)
// and decompile it with raised limits. For functions whose jump tables Ghidra did not follow, so it
// cut the body short (e.g. MetaExpression::GenerateDescription). Changes stay in memory (run the
// project read-only). args: <out.c> <entry hex (Ghidra address)> <size hex>
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import java.io.*;

public class DecompRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = toAddr(Long.parseLong(a[1], 16));
        Address end = start.add(Long.parseLong(a[2], 16) - 1);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function f = fm.getFunctionAt(start);
        String name = f.getName(true);
        java.util.List<Address> drop = new java.util.ArrayList<>();
        java.util.Iterator<Function> it = fm.getFunctionsOverlapping(new AddressSet(start, end));
        while (it.hasNext()) {
            Function g = it.next();
            if (!g.getEntryPoint().equals(start)) drop.add(g.getEntryPoint());
        }
        for (Address d : drop) fm.removeFunction(d);
        disassemble(start);
        AddressSet body = new AddressSet(start, end);
        f.setBody(body);
        DecompInterface di = new DecompInterface();
        DecompileOptions o = new DecompileOptions();
        o.grabFromProgram(currentProgram);
        o.setMaxPayloadMBytes(1024);
        o.setMaxInstructions(1000000);
        di.setOptions(o);
        di.openProgram(currentProgram);
        DecompileResults r = di.decompileFunction(f, 1800, monitor);
        try (PrintWriter c = new PrintWriter(new FileWriter(a[0]))) {
            c.println("// ==== " + f.getEntryPoint() + " " + name);
            if (r != null && r.decompileCompleted()) c.println(r.getDecompiledFunction().getC());
            else c.println("// DECOMPILE FAILED: " + (r == null ? "" : r.getErrorMessage()));
        }
    }
}
