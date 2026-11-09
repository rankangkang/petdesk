#pragma once
#include <QImage>
#import <Cocoa/Cocoa.h>

// Returns nil if the bundled portrait cannot be decoded.
NSImage *PetDeskStatusPortrait(void);
NSImage *PetDeskStatusImage(const QImage &image);

@interface PetDeskStatusTarget : NSObject <NSMenuDelegate>
- (void)performAction:(NSMenuItem *)item;
@end
