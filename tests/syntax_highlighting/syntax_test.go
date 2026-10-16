// Syntax highlighting test for Go: packages, imports, types, methods,
// interfaces, generics, goroutines, channels, defer and error handling.
package main

import (
	"errors"
	"fmt"
	"os"
	"strings"
	"sync"
)

//go:generate stringer -type=Color

// Color is an enumeration of colors.
type Color int

const (
	Red Color = iota
	Green
	Blue
)

const Pi = 3.14159
const mask = 0x1F | 0b1010 ^ 0o17

// Shape describes a geometric shape.
type Shape interface {
	Area() float64
	fmt.Stringer
}

type Rectangle struct {
	Width, Height float64
	Label         string `json:"label"`
	next          *Rectangle
}

func (r *Rectangle) Area() float64 {
	return r.Width * r.Height
}

func (r Rectangle) String() string {
	return fmt.Sprintf("rect %.2fx%.2f", r.Width, r.Height)
}

type Stack[T any] struct {
	items []T
}

func (s *Stack[T]) Push(v T) {
	s.items = append(s.items, v)
}

func Map[T, U any](xs []T, f func(T) U) []U {
	out := make([]U, 0, len(xs))
	for _, x := range xs {
		out = append(out, f(x))
	}
	return out
}

var ErrEmpty = errors.New("empty input")

func parse(input string) (int, error) {
	if input == "" {
		return 0, ErrEmpty
	}
	n := 0
	for i, r := range input {
		switch {
		case r >= '0' && r <= '9':
			n = n*10 + int(r-'0')
		case i > 3:
			break
		default:
			return 0, fmt.Errorf("bad rune %q at %d", r, i)
		}
	}
	return n, nil
}

func worker(id int, jobs <-chan int, results chan<- int, wg *sync.WaitGroup) {
	defer wg.Done()
	for job := range jobs {
		results <- job * 2
	}
	_ = id
}

func main() {
	raw := `raw string
with "quotes" and \n not escaped`
	msg := "tab\tnewline\n" + raw
	fmt.Println(msg, len(msg), Red, Pi, mask)

	jobs := make(chan int, 10)
	results := make(chan int, 10)
	var wg sync.WaitGroup
	for w := 1; w <= 3; w++ {
		wg.Add(1)
		go worker(w, jobs, results, &wg)
	}
	for j := 1; j <= 5; j++ {
		jobs <- j
	}
	close(jobs)
	wg.Wait()

	s := &Stack[string]{}
	s.Push(strings.ToUpper("go"))
	doubled := Map([]int{1, 2, 3}, func(x int) int { return x * 2 })
	fmt.Println(doubled, s.items)

	var shape Shape = &Rectangle{Width: 2, Height: 3}
	if rect, ok := shape.(*Rectangle); ok && rect.Area() > 1 {
		fmt.Println("area", rect.Area())
	} else if rect == nil {
		os.Exit(1)
	}

	m := map[string][]int{"a": {1, 2}, "b": nil}
	delete(m, "b")

	ch := make(chan string)
	select {
	case v := <-ch:
		fmt.Println(v)
	default:
	}

	defer func() {
		if r := recover(); r != nil {
			fmt.Println("recovered:", r)
		}
	}()

	x, err := parse("42")
	if err != nil {
		panic(err)
	}
	x <<= 2
	x &^= 1
	fmt.Printf("%d %v %t\n", x, nil, true)
	goto done
done:
	fmt.Println("done")
}
