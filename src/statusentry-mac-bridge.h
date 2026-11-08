#pragma once
#import <Cocoa/Cocoa.h>

// Returns nil if the bundled portrait cannot be decoded.
NSImage *PetDeskStatusPortrait(void);

@interface PetDeskStatusTarget : NSObject <NSMenuDelegate>
- (void)performAction:(NSMenuItem *)item;
@end
