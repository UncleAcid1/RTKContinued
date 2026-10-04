// Pre-analysis: disable the "discovered" non-returning heuristic, which wrongly
// marks operator_new, StringTable::GetString etc. as noreturn and truncates ~1900 functions.
//@category RTK
import ghidra.app.script.GhidraScript;

public class PreAnalysisOptions extends GhidraScript {
    @Override
    public void run() throws Exception {
        setAnalysisOption(currentProgram, "Non-Returning Functions - Discovered", "false");
    }
}
