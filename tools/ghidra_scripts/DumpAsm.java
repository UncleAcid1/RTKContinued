// Dump disassembly (with resolved PC-relative literal values) for functions whose name matches a regex.
// args: <regex> <outfile>
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.regex.*;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        Pattern pat = Pattern.compile(getScriptArgs()[0]);
        Memory mem = currentProgram.getMemory();
        try (PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[1]))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (!pat.matcher(f.getName(true)).find()) continue;
                w.println("==== " + f.getEntryPoint() + " " + f.getName(true) + " " + f.getSignature());
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                    StringBuilder sb = new StringBuilder(ins.getAddress() + "  " + ins);
                    for (Address ref : ins.getReferencesFrom().length > 0 ? new Address[]{ins.getReferencesFrom()[0].getToAddress()} : new Address[0]) {
                        if (!ref.isMemoryAddress()) continue;
                        Function cf = getFunctionAt(ref);
                        if (cf != null) { sb.append("    ; -> ").append(cf.getName(true)); continue; }
                        try {
                            int v = mem.getInt(ref);
                            sb.append(String.format("    ; [%s] = 0x%08x (%g)", ref, v, Float.intBitsToFloat(v)));
                        } catch (Exception e) { }
                    }
                    w.println(sb);
                }
            }
        }
    }
}
