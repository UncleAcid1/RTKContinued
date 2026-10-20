// DecompRangeAddr for a slice of a function too big for the decompiler (Ghidra hangs on the 69 KB
// PlayerCompetitionWindow::UpdateContents): drops the functions over the range, creates one at the
// slice start and decompiles that (the registers live at the start show as unaff_*).
// args: <out.c> <slice start hex (Ghidra address)> <size hex>
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import java.io.*;

public class DecompSlice extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = toAddr(Long.parseLong(a[1], 16));
        Address end = start.add(Long.parseLong(a[2], 16) - 1);
        FunctionManager fm = currentProgram.getFunctionManager();
        java.util.Iterator<Function> it0 = fm.getFunctionsOverlapping(new AddressSet(start, end)); java.util.List<Address> d0 = new java.util.ArrayList<>(); while (it0.hasNext()) d0.add(it0.next().getEntryPoint()); for (Address d : d0) fm.removeFunction(d); disassemble(start); Function f = createFunction(start, "pvp_slice");
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
        o.setMaxInstructions(1000000); o.setDefaultTimeout(1200);
        di.setOptions(o);
        di.openProgram(currentProgram);
        DecompileResults r = di.decompileFunction(f, 1800, monitor);
        try (PrintWriter c = new PrintWriter(new FileWriter(a[0]))) {
            c.println("// ==== " + f.getEntryPoint() + " " + name);
            if (r != null && r.decompileCompleted()) {
                for (ghidra.app.decompiler.ClangLine line :
                     ghidra.app.decompiler.component.DecompilerUtils.toLines(r.getCCodeMarkup())) {
                    Address min = null;
                    for (int i = 0; i < line.getNumTokens(); ++i) {
                        Address t = line.getToken(i).getMinAddress();
                        if (t != null && (min == null || t.compareTo(min) < 0)) min = t;
                    }
                    StringBuilder sb = new StringBuilder();
                    for (int i = 0; i < line.getIndent(); ++i) sb.append(' ');
                    for (int i = 0; i < line.getNumTokens(); ++i) sb.append(line.getToken(i).getText());
                    c.println((min == null ? "--------" : String.format("%08x", min.getOffset())) + " " + sb);
                }
            }
            else c.println("// DECOMPILE FAILED: " + (r == null ? "" : r.getErrorMessage()));
        }
    }
}
