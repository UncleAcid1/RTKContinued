// LoadSampling: the start-up's load steps, reported to the platform (FileManager::LoadStep: on
// Android, Java's load-time sampling). The steps are numbers the loaders pass; 0x400 ends it.
#pragma once

namespace LoadSampling {
void Init();                    // @0x1b19b0 (main_Loop_Init's start)
void LoadStep(unsigned step);   // @0x1b19c8 a new step is passed on once; nothing after 0x400
}  // namespace LoadSampling
