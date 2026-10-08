// Decompile functions matching a regex with raised decompiler limits (payload, instructions) and
// write C to <outdir>/<base>.c. For big functions whose default export drops code
// ("Heritage AFTER dead removal"). args: <outdir> <name regex> <base name>
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportDecompBig extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        java.util.regex.Pattern pat = java.util.regex.Pattern.compile(a[1]);
        DecompInterface di = new DecompInterface();
        DecompileOptions o = new DecompileOptions();
        o.grabFromProgram(currentProgram);
        o.setMaxPayloadMBytes(512);
        o.setMaxInstructions(400000);
        o.setDefaultTimeout(600);
        di.setOptions(o);
        di.openProgram(currentProgram);
        try (PrintWriter c = new PrintWriter(new FileWriter(a[0] + "/" + a[2] + ".c"))) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                if (f.isThunk() || f.isExternal() || !pat.matcher(f.getName(true)).find()) continue;
                DecompileResults r = di.decompileFunction(f, 600, monitor);
                c.println("// ==== " + f.getEntryPoint() + " " + f.getName(true));
                if (r != null && r.decompileCompleted()) c.println(r.getDecompiledFunction().getC());
                else c.println("// DECOMPILE FAILED: " + (r == null ? "" : r.getErrorMessage()));
            }
        }
    }
}
