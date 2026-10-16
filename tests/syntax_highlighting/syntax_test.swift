// Syntax highlighting test for Swift: imports, protocols, structs, classes,
// enums with associated values, extensions, generics, closures, optionals,
// error handling, property wrappers, string interpolation and async code.
import Foundation

/// A shape, documented with a doc comment.
protocol Shape: CustomStringConvertible {
    var area: Double { get }
    func scaled(by factor: Double) -> Self
}

enum Color: String {
    case red, green
    case custom(r: Int, g: Int, b: Int)
}

struct Point: Equatable {
    var x: Double = 0.0
    var y: Double = 0.0

    static let origin = Point()

    func distance(to other: Point) -> Double {
        let dx = x - other.x
        let dy = y - other.y
        return (dx * dx + dy * dy).squareRoot()
    }
}

final class Circle: Shape {
    private(set) var radius: Double
    var center = Point()

    init(radius: Double) {
        self.radius = radius
    }

    var area: Double {
        return Double.pi * radius * radius
    }

    var description: String { "Circle(r: \(radius))" }

    func scaled(by factor: Double) -> Circle {
        Circle(radius: radius * factor)
    }
}

extension Array where Element == Int {
    func total() -> Int {
        reduce(0, +)
    }
}

struct Stack<Element> {
    private var items: [Element] = []

    mutating func push(_ item: Element) {
        items.append(item)
    }

    mutating func pop() -> Element? {
        items.popLast()
    }
}

enum ParseError: Error {
    case invalidDigit(Character)
    case overflow
}

func parse(_ input: String) throws -> Int {
    var result = 0
    for character in input {
        guard let digit = character.wholeNumberValue else {
            throw ParseError.invalidDigit(character)
        }
        result = result * 10 + digit
    }
    return result
}

func describe(_ value: Any) -> String {
    switch value {
    case let number as Int where number > 10:
        return "big \(number)"
    case is String:
        return "text"
    case 0...5:
        return "small"
    default:
        return "other"
    }
}

@MainActor
func fetch(from url: URL) async throws -> Data {
    let (data, _) = try await URLSession.shared.data(from: url)
    return data
}

let numbers = [1, 2, 3, 4, 5]
let doubled = numbers.map { $0 * 2 }.filter { $0 > 4 }
let total = numbers.reduce(0) { acc, n in acc + n }
let optionalName: String? = nil
let name = optionalName ?? "anonymous"
let mask: UInt8 = 0b1010_0101 | 0xF0 & ~0x0F
let hex = 0xFF_FF
let ratio = 3.14e-2
let greeting = "Hello, \(name)!\n\tTab \"quoted\""
let multiline = """
    Multi-line
    string with \(total)
    """
var stack = Stack<Int>()
stack.push(42)
if let top = stack.pop(), top > 40 {
    print("top is \(top)")
} else if stack.pop() == nil {
    print("empty")
}

do {
    let value = try parse("1234")
    print(value)
} catch ParseError.invalidDigit(let character) {
    print("bad digit \(character)")
} catch {
    print("error: \(error)")
}

var counter = 0
while counter < 3 {
    counter += 1
    if counter == 2 { continue }
}
repeat {
    counter -= 1
} while counter > 0

defer { print("done") }
print(describe(counter), Circle(radius: 2).area, Color.custom(r: 1, g: 2, b: 3))
