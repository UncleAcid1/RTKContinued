// Decompile one function whose ARM jump table (`addls pc,pc,rN,lsl #2` followed by a table of
// branches) Ghidra did not recover: the function is re-created over its whole range (as
// DecompRange does) and each switch gets a jump-table override whose targets are the table's
// entries. Changes stay in memory (run the project read-only).
// args: <out.c> <entry hex (Ghidra address)> <size hex> (<switch instruction hex> <case count>)...
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.JumpTable;
import java.io.*;
import java.util.*;

public class SwitchDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = toAddr(Long.parseLong(a[1], 16));
        Address end = start.add(Long.parseLong(a[2], 16) - 1);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function f = fm.getFunctionAt(start);
        String name = f.getName(true);
        List<Address> drop = new ArrayList<>();
        Iterator<Function> it = fm.getFunctionsOverlapping(new AddressSet(start, end));
        while (it.hasNext()) {
            Function g = it.next();
            if (!g.getEntryPoint().equals(start)) drop.add(g.getEntryPoint());
        }
        for (Address d : drop) fm.removeFunction(d);
        for (int i = 3; i + 1 < a.length; i += 2) {
            Address sw = toAddr(Long.parseLong(a[i], 16));
            int n = Integer.parseInt(a[i + 1]);
            ArrayList<Address> dests = new ArrayList<>();
            for (int k = 0; k < n; ++k) {
                Address d = sw.add(8 + 4L * k);
                disassemble(d);
                dests.add(d);
            }
            new JumpTable(sw, dests, true, 0).writeOverride(f);
        }
        disassemble(start);
        // Everything reachable in the range: disassemble what is still undefined.
        for (Address p = start; p.compareTo(end) < 0; p = p.add(4))
            if (getInstructionAt(p) == null && getDataAt(p) == null) disassemble(p);
        f.setBody(new AddressSet(start, end));
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
