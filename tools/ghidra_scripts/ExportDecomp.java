// Decompile every function and write C to <outdir>/decomp.c, plus a symbol/address index.
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs().length > 0 ? getScriptArgs()[0] : ".";
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter c = new PrintWriter(new FileWriter(outDir + "/decomp.c"));
             PrintWriter idx = new PrintWriter(new FileWriter(outDir + "/functions.tsv"))) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            int n = 0, fail = 0;
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                if (f.isThunk() || f.isExternal()) continue;
                idx.println(f.getEntryPoint() + "\t" + f.getBody().getNumAddresses() + "\t" + f.getName(true));
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                c.println("// ==== " + f.getEntryPoint() + " " + f.getName(true));
                if (r != null && r.decompileCompleted()) c.println(r.getDecompiledFunction().getC());
                else { c.println("// DECOMPILE FAILED"); fail++; }
                if (++n % 1000 == 0) println("decompiled " + n);
            }
            println("done: " + n + " functions, " + fail + " failed");
        }
    }
}
