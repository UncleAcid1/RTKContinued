// ErrorReporter: the game's log lines. In 5.11 they go to the console only (Print is a printf); the
// on-screen line list Update fades is filled by nothing the game calls (its Add/Init are with the
// online and admin parts), so it stays empty.
#pragma once

namespace ErrorReporter {

// @0x16877c: the text, then a line break unless it has one.
void Print(const char* text, float r = 1.f, float g = 1.f, float b = 1.f);
// @0xe88c4: printf-style, up to 0x3ff characters, red.
void Printf(const char* fmt, ...);
// @0xf228c: the same in a colour.
void Printf(float r, float g, float b, const char* fmt, ...);
// @0x168638 (the main loop, before the game update): the lines' times count down; the oldest goes
// when its time runs out and the rest move up (16 px apart).
void Update(float dt);

}  // namespace ErrorReporter
