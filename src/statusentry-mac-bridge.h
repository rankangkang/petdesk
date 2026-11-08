#pragma once
#import <Cocoa/Cocoa.h>

@interface PetDeskStatusTarget : NSObject <NSMenuDelegate>
- (void)performAction:(NSMenuItem *)item;
@end
