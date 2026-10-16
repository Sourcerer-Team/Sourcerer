// Syntax highlighting test for F#: modules, types, functions, literals,
// operators, computation expressions, attributes and preprocessor blocks.
module Sourcerer.SyntaxTest

open System
open System.IO
open System.Text

#if DEBUG
#nowarn "9"
let buildMode = "debug"
#else
let buildMode = "release"
#endif

/// A geometric shape, documented with an XML doc comment.
type Shape =
    | Circle of radius: float
    | Rectangle of width: float * height: float
    | Empty

type Point = { X: float; Y: float }

type Animal() =
    member val Name = "unnamed" with get, set
    abstract Speak : unit -> string
    default this.Speak() = "..."

type IGreeter =
    abstract Greet : name: string -> string

[<Struct>]
type Vector =
    val X : float
    val Y : float
    new(x, y) = { X = x; Y = y }

type Unit = Meter
[<Measure>] type m

exception ParseError of line: int * message: string

let area shape =
    match shape with
    | Circle r -> System.Math.PI * r * r
    | Rectangle(w, h) when w > 0.0 -> w * h
    | Empty -> 0.0

let rec fib n =
    if n < 2 then n
    else fib (n - 1) + fib (n - 2)

let inline square x = x * x
let (|Even|Odd|) n = if n % 2 = 0 then Even else Odd

let numbers = [ 1; 2; 0x1F; 0b1010; 3L; 4.5; 1.0e-3; 7uy; 9M ]
let chars = [ 'a'; '\n'; '\u0041' ]
let text = "tab\tquoted \"text\""
let verbatim = @"C:\Program Files\Sourcerer"
let triple = """multi
line with "quotes" inside"""
let formatted = $"Hello {buildMode}, 2 + 2 = {2 + 2}"
let quotation = <@ 1 + 2 @>

let pipeline =
    [ 1 .. 10 ]
    |> List.filter (fun x -> x % 2 = 0)
    |> List.map (fun x -> x * square x)
    |> List.sum

let logicalOps = not (true && false) || (1 <> 2)
let shifted = (1 <<< 4) ||| 0xFF &&& ~~~0

let rec loop i acc =
    if i > 5 then acc
    else
        printfn "Iteration %d" i
        loop (i + 1) (acc + i)

let tryParse (input: string) =
    try
        int input |> Some
    with
    | :? FormatException as ex -> None
    | ex -> raise (ParseError(0, ex.Message))
    finally
        printfn "parsed %s" input

let sequences =
    seq {
        for i in 1 .. 3 do
            yield i * 2
        while false do
            yield! []
    }

let asyncWork =
    async {
        let! data = async { return "payload" }
        use reader = new StringReader(data)
        do! Async.Sleep 10
        return reader.ReadToEnd()
    }

let describe (p: Point) =
    let { X = x; Y = y } = p
    sprintf "(%g, %g)" x y

let lookup (dict: Collections.Generic.Dictionary<string, int>) key =
    match dict.TryGetValue key with
    | true, value -> value
    | _ -> -1

let mutable counter = 0
counter <- counter + 1

let withRecord = { X = 1.0; Y = 2.0 } |> fun p -> { p with Y = 3.0 }

[<EntryPoint>]
let main argv =
    printfn "%s %s" buildMode (String.Join(", ", argv))
    describe { X = 1.0; Y = 2.0 } |> printfn "%s"
    0
