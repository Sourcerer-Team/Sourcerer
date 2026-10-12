// Syntax highlighting test for Scala: packages, imports, classes, traits,
// objects, case classes, enums, pattern matching, generics, implicits and
// string interpolation.
package sourcerer.syntax

import scala.collection.mutable.{ArrayBuffer, Map => MutableMap}
import scala.util.Try

/** A shape, documented with a Scaladoc comment. */
sealed trait Shape {
  def area: Double
  def name: String = getClass.getSimpleName
}

final case class Circle(radius: Double) extends Shape {
  override def area: Double = math.Pi * radius * radius
}

case class Rectangle(width: Double, height: Double) extends Shape {
  def area: Double = width * height
}

object Shape {
  val Unit: Double = 1.0
  private[syntax] var counter: Int = 0
  lazy val origin = Circle(0.0)

  def describe(s: Shape): String = s match {
    case Circle(r) if r > 10 => s"big circle $r"
    case Rectangle(w, h)     => f"rect $w%.2f x $h%.2f"
    case _: Shape            => "other"
  }
}

trait Greeter[T] {
  def greet(who: T): String
  def twice(who: T): String = greet(who) + greet(who)
}

class Polite(prefix: String = "Dear") extends Greeter[String] {
  def greet(who: String): String = s"$prefix $who, hello!"
}

abstract class Animal(val legs: Int) {
  protected def sound: String
}

enum Color:
  case Red, Green
  case Custom(rgb: Int)

given intOrdering: Ordering[Int] = Ordering.Int

extension (n: Int)
  def isEven: Boolean = n % 2 == 0

implicit class RichString(val s: String) extends AnyVal {
  def shout: String = s.toUpperCase + "!"
}

// Higher-kinded types, context bounds and type lambdas.
def maxOf[A: Ordering](xs: Seq[A]): Option[A] =
  if (xs.isEmpty) None else Some(xs.max)

type Pair[A] = (A, A)
type Fn = [X] => X => X

// Collections, for-comprehensions and a tail-recursive helper.
val squares = for {
  i <- 1 to 10 if i % 2 == 0
  j = i * i
} yield j

@annotation.tailrec
def sumTo(n: Long, acc: Long = 0L): Long =
  if (n == 0) acc else sumTo(n - 1, acc + n)

val parsed: Try[Int] = Try("42".toInt).recover { case _: NumberFormatException => -1 }

val literals = List(42, 0x1F, 3.14, 2.5f, 'c', "esc\t\"quoted\"", true, null, ())
val multiline = """Triple "quoted"
  string with ${ 1 + 1 } not interpolated"""
val interpolated = s"sum = ${sumTo(10)}, parsed = ${parsed.getOrElse(0)}"

val map = MutableMap("a" -> 1, "b" -> 2)
val buffer = ArrayBuffer.empty[Int]
map.foreach { case (k, v) => buffer += v }

// Operators, lambdas and partial functions.
val add: (Int, Int) => Int = _ + _
val pf: PartialFunction[Int, String] = { case 1 => "one"; case _ => "many" }
val combined = List(1, 2, 3).map(x => x * 2).filter(_ > 2).foldLeft(0)(_ + _)
val bits = (7 & 3) | (1 << 4) ^ ~0 >>> 2

def process(input: Option[String]): Int =
  input match {
    case Some(text) if text.nonEmpty => text.length
    case Some(_) | None              => 0
  }

@main def run(args: String*): Unit =
  println(Shape.describe(Circle(3)))
  println(new Polite().greet("Ada"))
  while (Shape.counter < 3) { Shape.counter += 1 }
  try {
    throw new IllegalStateException("boom")
  } catch {
    case e: IllegalStateException => println(e.getMessage)
  } finally {
    println("done")
  }
