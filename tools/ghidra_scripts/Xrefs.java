// List references to given addresses (and to GOT slots pointing at them). args: <hexaddr>...
//@category RTK
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;

public class Xrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Address t = toAddr(a);
            for (Reference r : getReferencesTo(t)) {
                Function f = getFunctionContaining(r.getFromAddress());
                Instruction ins = getInstructionAt(r.getFromAddress());
                println("XREF " + a + " <- " + r.getFromAddress() + " " + r.getReferenceType() + " " + (f == null ? "?" : f.getName(true)) + " | " + ins);
            }
        }
    }
}
