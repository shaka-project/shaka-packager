// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built (currently 10.6).
// If you call any function from this header, be sure to check at runtime for
// respondsToSelector: before calling these functions (else your code will crash
// on older OS X versions that chrome still supports).

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#import <AppKit/AppKit.h>

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
enum {
  NSEventPhaseNone        = 0, // event not associated with a phase.
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

enum {
  NSEventSwipeTrackingLockDirection = 0x1 << 0,
  NSEventSwipeTrackingClampGestureAmount = 0x1 << 1,
};
typedef NSUInteger NSEventSwipeTrackingOptions;

@interface NSEvent (LionSDK)
+ (BOOL)isSwipeTrackingFromScrollEventsEnabled;

- (NSEventPhase)phase;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options
          dampenAmountThresholdMin:(CGFloat)minDampenThreshold
                               max:(CGFloat)maxDampenThreshold
                      usingHandler:(void (^)(CGFloat gestureAmount,
                                             NSEventPhase phase,
                                             BOOL isComplete,
                                             BOOL *stop))trackingHandler;

- (BOOL)isDirectionInvertedFromDevice;

@end

@interface CALayer (LionAPI)
- (CGFloat)contentsScale;
- (void)setContentsScale:(CGFloat)contentsScale;
@end

@interface NSScreen (LionSDK)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

@interface NSWindow (LionSDK)
- (CGFloat)backingScaleFactor;
@end
#endif  // MAC_OS_X_VERSION_10_7

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
