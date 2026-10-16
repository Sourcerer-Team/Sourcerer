// Syntax highlighting test for Kotlin: packages, imports, classes, data
// classes, interfaces, objects, extensions, generics, lambdas, coroutines,
// string templates and null safety.
package sourcerer.syntax

import kotlin.math.PI
import java.io.File as JFile

/** A shape, documented with KDoc. */
sealed interface Shape {
    val name: String
    fun area(): Double
}

data class Circle(val radius: Double) : Shape {
    override val name: String = "circle"
    override fun area(): Double = PI * radius * radius
}

class Rectangle(private val width: Double, private var height: Double = 1.0) : Shape {
    override val name get() = "rectangle"

    override fun area() = width * height

    companion object {
        const val UNIT = 1.0
        @JvmStatic
        fun square(side: Double) = Rectangle(side, side)
    }
}

object Registry {
    private val shapes = mutableListOf<Shape>()

    fun register(shape: Shape): Registry = apply { shapes.add(shape) }

    val total: Double
        get() = shapes.sumOf { it.area() }
}

enum class Color(val hex: Long) {
    RED(0xFF0000),
    GREEN(0x00FF00),
    BLUE(0x0000FF);

    fun brightness(): Int = (hex shr 16 and 0xFF).toInt()
}

fun String.shout(): String = uppercase() + "!"

fun <T : Comparable<T>> maxOf(items: List<T>): T? {
    if (items.isEmpty()) return null
    var best = items[0]
    for (item in items) {
        if (item > best) best = item
    }
    return best
}

fun describe(value: Any?): String = when (value) {
    null -> "nothing"
    is Int -> "int $value"
    is String -> "string of ${value.length} chars"
    in 1..10 -> "small"
    else -> "other"
}

fun parse(input: String?): Int {
    val number = input?.toIntOrNull() ?: return -1
    return try {
        number * 2
    } catch (e: NumberFormatException) {
        throw IllegalArgumentException("bad input: $input", e)
    } finally {
        println("parsed")
    }
}

suspend fun fetch(url: String): String {
    delay(10)
    return "data from $url"
}

fun main(args: Array<String>) {
    val numbers = listOf(1, 2, 3, 4, 5)
    val evens = numbers.filter { it % 2 == 0 }.map { it * it }
    val doubled = numbers.fold(0) { acc, n -> acc + n * 2 }
    val labels = numbers.associateWith { "n$it" }

    val raw = """
        |multi-line "raw" string
        |with ${numbers.size} items
    """.trimMargin()
    val escaped = "tab\t\"quoted\" A \$literal"
    val char = 'k'
    val big = 1_000_000L
    val ratio = 2.5f

    var nullable: String? = null
    nullable?.let { println(it) }
    val length = nullable?.length ?: 0
    nullable!!.length

    val rect = Rectangle.square(2.0).apply { height = 3.0 }
    Registry.register(Circle(1.0)).register(rect)
    println("total=${Registry.total}, colors=${Color.values().size}")

    if (evens.size > 1 && doubled >= 10 || !numbers.isEmpty()) {
        println(describe(evens.first()))
    }

    for (i in 0 until numbers.size step 2) {
        println(numbers[i])
    }

    val run = { x: Int -> x + length }
    println(run(3).toString() + labels.keys)
    println(args.size + char.code + big.toInt() + ratio.toInt())
    println(escaped + raw + doubled)
    println(JFile(".").name)
}
