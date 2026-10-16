//! Syntax highlighting test for Zig: imports, containers, functions, errors,
//! optionals, comptime, switch, defer, slices and built-in calls.

const std = @import("std");
const builtin = @import("builtin");
const Allocator = std.mem.Allocator;

/// Maximum number of items the buffer holds.
pub const max_items: usize = 64;
const magic: u32 = 0xDEAD_BEEF;
const ratio: f64 = 3.14159e-2;

pub const Color = enum(u8) {
    red = 1,
    green,
    blue,
};

const Rect = struct { w: f32, h: f32 };

pub const Shape = union(enum) {
    circle: f32,
    rect: Rect,
    none,
};

const Point = struct {
    x: i32 = 0,
    y: i32 = 0,

    pub fn distance(self: Point, other: Point) f64 {
        const dx: f64 = @floatFromInt(self.x - other.x);
        const dy: f64 = @floatFromInt(self.y - other.y);
        return @sqrt(dx * dx + dy * dy);
    }
};

pub const ParseError = error{ InvalidDigit, Overflow };

fn parseDigits(input: []const u8) ParseError!u32 {
    var result: u32 = 0;
    for (input) |c| {
        if (c < '0' or c > '9') return error.InvalidDigit;
        result = std.math.mul(u32, result, 10) catch return error.Overflow;
        result += c - '0';
    }
    return result;
}

fn find(items: []const i32, needle: i32) ?usize {
    var index: usize = 0;
    for (items) |item| {
        if (item == needle) return index;
        index += 1;
    }
    return null;
}

fn describe(shape: Shape) []const u8 {
    return switch (shape) {
        .circle => |radius| if (radius > 10.0) "big circle" else "circle",
        .rect => |r| blk: {
            if (r.w == r.h) break :blk "square";
            break :blk "rectangle";
        },
        .none => "nothing",
    };
}

fn fill(allocator: Allocator, count: usize) ![]u8 {
    const buffer = try allocator.alloc(u8, count);
    errdefer allocator.free(buffer);
    @memset(buffer, 'x');
    return buffer;
}

fn Stack(comptime T: type) type {
    return struct {
        items: [max_items]T = undefined,
        len: usize = 0,
        const Self = @This();

        pub fn push(self: *Self, value: T) !void {
            if (self.len == max_items) return error.OutOfMemory;
            self.items[self.len] = value;
            self.len += 1;
        }

        pub fn pop(self: *Self) ?T {
            if (self.len == 0) return null;
            self.len -= 1;
            return self.items[self.len];
        }
    };
}

test "stack push and pop" {
    const I32Stack = Stack(i32);
    var stack: I32Stack = .{};
    try stack.push(magic_value());
    try std.testing.expectEqual(@as(?i32, 42), stack.pop());
}

fn magic_value() i32 {
    return 42;
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    const Gpa = std.heap.GeneralPurposeAllocator(.{});
    var gpa: Gpa = .{};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const text = "tab\tquote\" unicode \u{1F600}\n";
    const multi =
        \\line one
        \\line two
    ;
    const raw_bytes = &[_]u8{ 0x41, 0x42, 'C' };
    const bytes = fill(allocator, 16) catch |err| {
        std.debug.print("error: {s}\n", .{@errorName(err)});
        return err;
    };
    defer allocator.free(bytes);

    var p = Point{ .x = 3, .y = 4 };
    const dist = p.distance(.{});
    const maybe: ?u8 = if (bytes.len > 8) bytes[0] else null;
    if (maybe) |value| {
        try stdout.print("first byte {d}\n", .{value});
    } else {
        try stdout.writeAll("empty\n");
    }

    const numbers = [_]i32{ 1, 2, 3, 4, 5 };
    const slice = numbers[1..4];
    var total: i64 = 0;
    for (slice) |n| {
        total += @as(i64, n);
    }

    var counter: usize = 0;
    while (counter < 3) : (counter += 1) {
        if (counter == 1) continue;
    }

    const color: Color = .green;
    const label = switch (color) {
        .red => "red",
        .green, .blue => "cool",
    };
    std.debug.print("{s}\n", .{label});

    const shifted = magic >> 4;
    const wrapped = @addWithOverflow(p.x, 1);
    _ = wrapped;
    _ = shifted;
    _ = dist;
    _ = raw_bytes;
    _ = text;
    _ = multi;
    _ = total;
    _ = find(&numbers, 3);
    _ = builtin.mode;
    try stdout.print("{s} {d:.2} {any}\n", .{ describe(.none), ratio, parseDigits("12") catch 0 });
}
