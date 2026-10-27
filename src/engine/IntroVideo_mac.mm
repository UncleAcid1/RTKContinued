#include "engine/IntroVideo.h"

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#include <SDL3/SDL.h>

namespace IntroVideo {

bool Play(SDL_Window* window, const std::string& path) {
    NSWindow* nsWindow = (__bridge NSWindow*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    NSString* file = [NSString stringWithUTF8String:path.c_str()];
    if (!nsWindow || ![[NSFileManager defaultManager] fileExistsAtPath:file]) return true;
    AVPlayer* player = [AVPlayer playerWithURL:[NSURL fileURLWithPath:file]];

    // A black view over the game's, the film fitted inside it (ShowVideoSplash's SurfaceView).
    NSView* content = nsWindow.contentView;
    NSView* view = [[NSView alloc] initWithFrame:content.bounds];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.wantsLayer = YES;
    view.layer.backgroundColor = NSColor.blackColor.CGColor;
    AVPlayerLayer* layer = [AVPlayerLayer playerLayerWithPlayer:player];
    layer.frame = view.bounds;
    layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    layer.videoGravity = AVLayerVideoGravityResizeAspect;
    [view.layer addSublayer:layer];
    [content addSubview:view positioned:NSWindowAbove relativeTo:nil];
    [player play];

    bool open = true, ended = false;
    while (!ended) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // ShowVideoSplash.onClick: a tap ends it (VideoEnded).
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_KEY_DOWN) ended = true;
            if (e.type == SDL_EVENT_QUIT) ended = true, open = false;
        }
        // onCompletion / onError: the film's end or a failure ends it too.
        AVPlayerItem* item = player.currentItem;
        if (!item || item.status == AVPlayerItemStatusFailed) ended = true;
        else if (item.status == AVPlayerItemStatusReadyToPlay && CMTIME_IS_NUMERIC(item.duration) &&
                 CMTimeCompare(item.currentTime, item.duration) >= 0)
            ended = true;
        SDL_Delay(10);
    }
    [player pause];
    [layer removeFromSuperlayer];
    [view removeFromSuperview];
    return open;
}

}  // namespace IntroVideo
