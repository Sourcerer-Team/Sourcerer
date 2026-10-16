// Syntax highlighting test for Objective-C++: imports, interfaces, protocols,
// categories, message sends, properties, blocks, @-literals, exceptions and
// the C and C++ code mixed in with them.
#import <Foundation/Foundation.h>
#include <vector>
#define MAX_ITEMS 64

@protocol Drawable <NSObject>
- (void)draw;
@optional
- (NSInteger)layer;
@end

@interface Shape : NSObject <Drawable> {
    CGFloat _area;
    NSString *_name;
}
@property (nonatomic, copy) NSString *name;
@property (readonly) CGFloat area;
+ (instancetype)shapeWithName:(NSString *)name;
- (instancetype)initWithName:(NSString *)name;
@end

@implementation Shape

@synthesize name = _name;

+ (instancetype)shapeWithName:(NSString *)name {
    return [[self alloc] initWithName:name];
}

- (instancetype)initWithName:(NSString *)name {
    self = [super init];
    if (self) {
        _name = [name copy];
        _area = 0.0;
    }
    return self;
}

- (void)draw {
    NSLog(@"Drawing %@ with area %.2f", self.name, self.area);
}

@end

@interface Shape (Printing)
- (void)printSummary;
@end

@implementation Shape (Printing)
- (void)printSummary {
    [self.name length];
}
@end

static int counter = 0;
static const char *greeting = "hello\tworld\n";

int sumTo(int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += i * 2;
    }
    return total;
}

void useBlocks(NSArray<NSString *> *items) {
    NSNumber *number = @42;
    NSDictionary *info = @{ @"name": @"sourcerer", @"count": @(MAX_ITEMS) };
    NSArray *list = @[ @1, @2, @3 ];
    SEL action = @selector(draw);
    void (^printer)(NSString *) = ^(NSString *text) {
        NSLog(@"%@", text);
    };
    [items enumerateObjectsUsingBlock:^(NSString *item, NSUInteger idx, BOOL *stop) {
        if (idx > 10) {
            *stop = YES;
        }
        printer(item);
    }];
    if ([number isKindOfClass:[NSNumber class]] && list.count > 2) {
        counter += 1;
    }
    @try {
        [NSException raise:@"Oops" format:@"bad %d", counter];
    } @catch (NSException *exception) {
        NSLog(@"caught %@", exception.reason);
    } @finally {
        counter = 0;
    }
}

@autoreleasepool {
    Shape *s = [Shape shapeWithName:@"circle"];
    [s draw];
    printf("%d %s\n", sumTo(10), greeting);
}
