// source/tests/libraries/string.apex
// Implementation of String Library Function Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: string length returns character count
function string_length()
    // basic ASCII tests
    len = string.length("Hello")
    assert_equal(len, 5, "string.length Hello")
    
    len = string.length("")
    assert_equal(len, 0, "string.length empty")
    
    len = string.length("a")
    assert_equal(len, 1, "string.length single char")
    
    len = string.length("ab")
    assert_equal(len, 2, "string.length two chars")
    
    len = string.length("abc")
    assert_equal(len, 3, "string.length three chars")
    
    // longer ASCII strings
    len = string.length("Hello World")
    assert_equal(len, 11, "string.length Hello World")
    
    len = string.length("The quick brown fox jumps over the lazy dog")
    assert_equal(len, 43, "string.length pangram")
    
    len = string.length("abcdefghijklmnopqrstuvwxyz")
    assert_equal(len, 26, "string.length alphabet lowercase")
    
    len = string.length("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
    assert_equal(len, 26, "string.length alphabet uppercase")
    
    len = string.length("0123456789")
    assert_equal(len, 10, "string.length digits")
    
    // whitespace strings
    len = string.length(" ")
    assert_equal(len, 1, "string.length single space")
    
    len = string.length("   ")
    assert_equal(len, 3, "string.length three spaces")
    
    len = string.length("\t")
    assert_equal(len, 1, "string.length tab")
    
    len = string.length("\n")
    assert_equal(len, 1, "string.length newline")
    
    len = string.length("\t\n")
    assert_equal(len, 2, "string.length tab newline")
    
    len = string.length(" \t\n ")
    assert_equal(len, 4, "string.length mixed whitespace")
    
    // special characters
    len = string.length("!@#$%^&*()")
    assert_equal(len, 10, "string.length special chars")
    
    len = string.length("...")
    assert_equal(len, 3, "string.length dots")
    
    len = string.length(",,,")
    assert_equal(len, 3, "string.length commas")
    
    len = string.length(";;;")
    assert_equal(len, 3, "string.length semicolons")
    
    len = string.length("::: ")
    assert_equal(len, 4, "string.length colons space")
    
    len = string.length("|\\/")
    assert_equal(len, 3, "string.length pipe slash backslash")
    
    // punctuation
    len = string.length("Hello, World!")
    assert_equal(len, 13, "string.length with punctuation")
    
    len = string.length("What's up?")
    assert_equal(len, 10, "string.length with apostrophe")
    
    len = string.length("\"quoted\"")
    assert_equal(len, 8, "string.length with quotes")
    
    len = string.length("(parentheses)")
    assert_equal(len, 13, "string.length with parentheses")
    
    len = string.length("[brackets]")
    assert_equal(len, 10, "string.length with brackets")
    
    len = string.length("\{braces\}")
    assert_equal(len, 8, "string.length with braces")
    
    // numbers and mixed content
    len = string.length("12345")
    assert_equal(len, 5, "string.length numbers 12345")
    
    len = string.length("1-2-3-4-5")
    assert_equal(len, 9, "string.length numbers with dashes")
    
    len = string.length("a1b2c3")
    assert_equal(len, 6, "string.length mixed alphanumeric")
    
    len = string.length("version 1.0")
    assert_equal(len, 11, "string.length version string")
    
    len = string.length("100%")
    assert_equal(len, 4, "string.length percentage")
    
    len = string.length("$100.50")
    assert_equal(len, 7, "string.length currency")
    
    // Unicode - Chinese
    len = string.length("你好")
    assert_equal(len, 2, "string.length chinese 2 chars")
    
    len = string.length("你好世界")
    assert_equal(len, 4, "string.length chinese 4 chars")
    
    len = string.length("你好世界你好")
    assert_equal(len, 6, "string.length chinese 6 chars")
    
    len = string.length("中文测试")
    assert_equal(len, 4, "string.length chinese test")
    
    // Unicode - Russian
    len = string.length("Привет")
    assert_equal(len, 6, "string.length russian Привет")
    
    len = string.length("Привет Мир")
    assert_equal(len, 10, "string.length russian Привет Мир")
    
    len = string.length("Здравствуйте")
    assert_equal(len, 12, "string.length russian Здравствуйте")
    
    len = string.length("русский текст")
    assert_equal(len, 13, "string.length russian text")
    
    // Unicode - Arabic
    len = string.length("مرحبا")
    assert_equal(len, 5, "string.length arabic مرحبا")
    
    len = string.length("السلام عليكم")
    assert_equal(len, 12, "string.length arabic السلام عليكم")
    
    len = string.length("عربي")
    assert_equal(len, 4, "string.length arabic عربي")
    
    // Unicode - Japanese
    len = string.length("こんにちは")
    assert_equal(len, 5, "string.length japanese こんにちは")
    
    len = string.length("日本語")
    assert_equal(len, 3, "string.length japanese 日本語")
    
    len = string.length("テスト")
    assert_equal(len, 3, "string.length japanese テスト")
    
    // Unicode - Korean
    len = string.length("안녕하세요")
    assert_equal(len, 5, "string.length korean 안녕하세요")
    
    len = string.length("한국어")
    assert_equal(len, 3, "string.length korean 한국어")
    
    // Unicode - Emoji
    len = string.length("😊")
    assert_equal(len, 1, "string.length emoji single")
    
    len = string.length("😊🌟😊")
    assert_equal(len, 3, "string.length emoji three")
    
    len = string.length("👍🏽")
    assert_equal(len, 2, "string.length emoji with skin tone")
    
    len = string.length("🎉🎊🎈")
    assert_equal(len, 3, "string.length celebration emojis")
    
    len = string.length("❤️💙💚")
    assert_equal(len, 4, "string.length heart emojis")
    
    // Mixed scripts
    len = string.length("Hello Привет")
    assert_equal(len, 12, "string.length mixed latin cyrillic")
    
    len = string.length("Hello 你好")
    assert_equal(len, 8, "string.length mixed latin chinese")
    
    len = string.length("Привет 你好")
    assert_equal(len, 9, "string.length mixed cyrillic chinese")
    
    len = string.length("Hello Привет 你好")
    assert_equal(len, 15, "string.length mixed three scripts")
    
    len = string.length("123 abc 你好")
    assert_equal(len, 10, "string.length mixed numbers latin chinese")
    
    len = string.length("😊 Hello 你好")
    assert_equal(len, 10, "string.length mixed emoji latin chinese")
    
    // Long strings
    len = string.length("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
    assert_equal(len, 50, "string.length 50 a's")
    
    len = string.length("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
    assert_equal(len, 100, "string.length 100 a's")
    
    len = string.length("abcabcabcabcabcabcabcabcabcabc")
    assert_equal(len, 30, "string.length repeated abc")
    
    len = string.length("012345678901234567890123456789")
    assert_equal(len, 30, "string.length repeated digits")
    
    // Strings with repeating patterns
    len = string.length("aaaa")
    assert_equal(len, 4, "string.length four a's")
    
    len = string.length("aabbaabb")
    assert_equal(len, 8, "string.length aabbaabb")
    
    len = string.length("abcabcabc")
    assert_equal(len, 9, "string.length abcabcabc")
    
    // Strings with mixed case
    len = string.length("Hello")
    assert_equal(len, 5, "string.length Hello mixed case")
    
    len = string.length("hELLO")
    assert_equal(len, 5, "string.length hELLO mixed case")
    
    len = string.length("HeLLo WoRLd")
    assert_equal(len, 11, "string.length HeLLo WoRLd")
    
    // File paths and URLs
    len = string.length("/home/user/file.txt")
    assert_equal(len, 19, "string.length file path")
    
    len = string.length("https://example.com")
    assert_equal(len, 19, "string.length URL")
    
    len = string.length("C:\\Users\\test")
    assert_equal(len, 13, "string.length windows path")
    
    len = string.length("../relative/path/")
    assert_equal(len, 17, "string.length relative path")
    
    // Email addresses
    len = string.length("user@example.com")
    assert_equal(len, 16, "string.length email")
    
    len = string.length("test.user@sub.domain.com")
    assert_equal(len, 24, "string.length long email")
    
    // Code snippets
    len = string.length("function test() \{ \}")
    assert_equal(len, 19, "string.length function")
    
    len = string.length("var x = 10;")
    assert_equal(len, 11, "string.length variable declaration")
    
    len = string.length("if (x > 5) \{ return true; \}")
    assert_equal(len, 27, "string.length if statement")
    
    // JSON-like strings
    len = string.length("\{\"key\": \"value\"\}")
    assert_equal(len, 16, "string.length JSON")
    
    len = string.length("[1, 2, 3]")
    assert_equal(len, 9, "string.length array")
    
    // HTML tags
    len = string.length("<div>content</div>")
    assert_equal(len, 18, "string.length HTML")
    
    len = string.length("<p class=\"test\">text</p>")
    assert_equal(len, 24, "string.length HTML with attributes")
    
    // Multi-line strings
    len = string.length("line1\nline2\nline3")
    assert_equal(len, 17, "string.length multiline")
    
    len = string.length("first\nsecond\nthird\nfourth")
    assert_equal(len, 25, "string.length four lines")
    
    // Strings with leading/trailing spaces
    len = string.length("  hello")
    assert_equal(len, 7, "string.length leading spaces")
    
    len = string.length("hello  ")
    assert_equal(len, 7, "string.length trailing spaces")
    
    len = string.length("  hello  ")
    assert_equal(len, 9, "string.length both spaces")
    
    len = string.length("\thello\t")
    assert_equal(len, 7, "string.length with tabs")
    
    len = string.length("\nhello\n")
    assert_equal(len, 7, "string.length with newlines")
    
    // Very long repeated Unicode
    len = string.length("你好你好你好你好你好")
    assert_equal(len, 10, "string.length repeated chinese")
    
    len = string.length("😊😊😊😊😊😊😊😊😊😊")
    assert_equal(len, 10, "string.length repeated emoji")
    
    len = string.length("ПриветПриветПривет")
    assert_equal(len, 18, "string.length repeated russian")

// test: convert string to lowercase
function string_lower()
    result = string.lower("HELLO")
    assert_equal(result, "hello", "string.lower HELLO")
    
    result = string.lower("Hello World")
    assert_equal(result, "hello world", "string.lower Hello World")

// test: convert string to uppercase
function string_upper()
    result = string.upper("hello")
    assert_equal(result, "HELLO", "string.upper hello")
    
    result = string.upper("Hello World")
    assert_equal(result, "HELLO WORLD", "string.upper Hello World")

// test: substring extraction with various scripts and edge cases
function string_slice()
    // basic slice positions
    result = string.slice("Hello World", 1, 1)
    assert_equal(result, "H", "string.slice single char H")
    
    result = string.slice("Hello World", 11, 11)
    assert_equal(result, "d", "string.slice single char d")
    
    result = string.slice("Hello World", 6, 6)
    assert_equal(result, " ", "string.slice space char")
    
    // slice entire string
    result = string.slice("complete", 1, 8)
    assert_equal(result, "complete", "string.slice entire string")
    
    result = string.slice("exact", 1, 5)
    assert_equal(result, "exact", "string.slice exact length")
    
    // slice with start beyond end
    result = string.slice("short", 10, 15)
    assert_equal(result, "", "string.slice start beyond end")
    
    result = string.slice("short", 6, 10)
    assert_equal(result, "", "string.slice start after last char")
    
    // slice with end beyond end
    result = string.slice("hello", 3, 100)
    assert_equal(result, "llo", "string.slice end far beyond")
    
    result = string.slice("hello", 5, 50)
    assert_equal(result, "o", "string.slice last char to beyond")
    
    // slice with start greater than end
    result = string.slice("hello", 5, 1)
    assert_equal(result, "", "string.slice start > end")
    
    result = string.slice("hello", 3, 2)
    assert_equal(result, "", "string.slice reversed range")
    
    // slice with negative start
    result = string.slice("hello", -1, 3)
    assert_equal(result, "hel", "string.slice negative start -1")
    
    result = string.slice("hello", -10, 5)
    assert_equal(result, "hello", "string.slice very negative start")
    
    // slice with zero start
    result = string.slice("hello", 0, 3)
    assert_equal(result, "hel", "string.slice zero start")
    
    result = string.slice("hello", 0, 0)
    assert_equal(result, "", "string.slice zero start and end")
    
    // slice empty string
    result = string.slice("", 1, 1)
    assert_equal(result, "", "string.slice empty string")
    
    result = string.slice("", 0, 5)
    assert_equal(result, "", "string.slice empty string range")
    
    result = string.slice("", -5, 5)
    assert_equal(result, "", "string.slice empty negative start")
    
    // unicode slice tests
    result = string.slice("你好世界", 1, 1)
    assert_equal(result, "你", "string.slice chinese single char")
    
    result = string.slice("你好世界", 4, 4)
    assert_equal(result, "界", "string.slice chinese last char")
    
    result = string.slice("你好世界", 2, 3)
    assert_equal(result, "好世", "string.slice chinese middle chars")
    
    result = string.slice("你好世界", 1, 4)
    assert_equal(result, "你好世界", "string.slice chinese entire")
    
    result = string.slice("你好世界", 5, 10)
    assert_equal(result, "", "string.slice chinese beyond end")
    
    result = string.slice("你好世界", -2, 2)
    assert_equal(result, "你好", "string.slice chinese negative start")
    
    // russian slice tests
    result = string.slice("Привет Мир", 1, 1)
    assert_equal(result, "П", "string.slice russian single char")
    
    result = string.slice("Привет Мир", 10, 10)
    assert_equal(result, "р", "string.slice russian last char")
    
    result = string.slice("Привет Мир", 7, 7)
    assert_equal(result, " ", "string.slice russian space")
    
    result = string.slice("Привет Мир", 8, 10)
    assert_equal(result, "Мир", "string.slice russian word")
    
    result = string.slice("Привет Мир", 1, 10)
    assert_equal(result, "Привет Мир", "string.slice russian entire")
    
    result = string.slice("Привет Мир", 11, 15)
    assert_equal(result, "", "string.slice russian beyond end")
    
    result = string.slice("Привет Мир", -3, 6)
    assert_equal(result, "Привет", "string.slice russian negative start")
    
    // arabic slice tests
    result = string.slice("السلام عليكم", 1, 1)
    assert_equal(result, "ا", "string.slice arabic first char")

    result = string.slice("السلام عليكم", 12, 12)
    assert_equal(result, "م", "string.slice arabic last char")

    result = string.slice("السلام عليكم", 7, 7)
    assert_equal(result, " ", "string.slice arabic space")

    result = string.slice("السلام عليكم", 1, 12)
    assert_equal(result, "السلام عليكم", "string.slice arabic entire")

    result = string.slice("السلام عليكم", 13, 20)
    assert_equal(result, "", "string.slice arabic beyond end")

    result = string.slice("السلام عليكم", -5, 8)
    assert_equal(result, "السلام ع", "string.slice arabic negative start")
    
    // mixed scripts slice
    result = string.slice("Hello Привет 你好", 1, 1)
    assert_equal(result, "H", "string.slice mixed first char")
    
    result = string.slice("Hello Привет 你好", 15, 15)
    assert_equal(result, "好", "string.slice mixed last char")
    
    result = string.slice("Hello Привет 你好", 6, 6)
    assert_equal(result, " ", "string.slice mixed space 1")
    
    result = string.slice("Hello Привет 你好", 13, 13)
    assert_equal(result, " ", "string.slice mixed space 2")
    
    result = string.slice("Hello Привет 你好", 1, 15)
    assert_equal(result, "Hello Привет 你好", "string.slice mixed entire")
    
    result = string.slice("Hello Привет 你好", 16, 20)
    assert_equal(result, "", "string.slice mixed beyond end")
    
    result = string.slice("Hello Привет 你好", -8, 5)
    assert_equal(result, "Hello", "string.slice mixed negative start")
    
    // emoji slice tests
    result = string.slice("😊🌟😊", 1, 1)
    assert_equal(result, "😊", "string.slice emoji first")
    
    result = string.slice("😊🌟😊", 2, 2)
    assert_equal(result, "🌟", "string.slice emoji middle")
    
    result = string.slice("😊🌟😊", 3, 3)
    assert_equal(result, "😊", "string.slice emoji last")
    
    result = string.slice("😊🌟😊", 1, 3)
    assert_equal(result, "😊🌟😊", "string.slice emoji entire")
    
    result = string.slice("😊🌟😊", 4, 5)
    assert_equal(result, "", "string.slice emoji beyond end")
    
    result = string.slice("😊🌟😊", -1, 2)
    assert_equal(result, "😊🌟", "string.slice emoji negative start")
    
    result = string.slice("😊🌟😊", 0, 1)
    assert_equal(result, "😊", "string.slice emoji zero start")
    
    // special characters slice
    result = string.slice("a.b.c.d", 2, 2)
    assert_equal(result, ".", "string.slice dot single")
    
    result = string.slice("a.b.c.d", 1, 7)
    assert_equal(result, "a.b.c.d", "string.slice dots entire")
    
    result = string.slice("path\\to\\file", 5, 5)
    assert_equal(result, "\\", "string.slice backslash single")
    
    result = string.slice("path\\to\\file", 9, 12)
    assert_equal(result, "file", "string.slice file after backslash")
    
    result = string.slice("line1\nline2\nline3", 6, 6)
    assert_equal(result, "\n", "string.slice newline single")
    
    result = string.slice("line1\nline2\nline3", 7, 11)
    assert_equal(result, "line2", "string.slice line2")
    
    result = string.slice("tab\tseparated\tvalues", 4, 4)
    assert_equal(result, "\t", "string.slice tab single")
    
    result = string.slice("tab\tseparated\tvalues", 15, 20)
    assert_equal(result, "values", "string.slice values after tab")
    
    // numbers and mixed content slice
    result = string.slice("1-2-3-4-5", 2, 2)
    assert_equal(result, "-", "string.slice dash single")
    
    result = string.slice("1-2-3-4-5", 5, 5)
    assert_equal(result, "3", "string.slice digit 3")
    
    result = string.slice("1-2-3-4-5", 1, 9)
    assert_equal(result, "1-2-3-4-5", "string.slice numbers entire")
    
    result = string.slice("a1b2c3", 2, 2)
    assert_equal(result, "1", "string.slice digit in mixed")
    
    result = string.slice("a1b2c3", 3, 4)
    assert_equal(result, "b2", "string.slice b2 in mixed")
    
    result = string.slice("version 1.0 version 2.0", 1, 7)
    assert_equal(result, "version", "string.slice first version")
    
    result = string.slice("version 1.0 version 2.0", 21, 23)
    assert_equal(result, "2.0", "string.slice 2.0")
    
    // slice with whitespace boundaries
    result = string.slice(" test", 2, 5)
    assert_equal(result, "test", "string.slice test after space")
    
    result = string.slice("test ", 1, 4)
    assert_equal(result, "test", "string.slice test before space")
    
    result = string.slice(" test ", 2, 5)
    assert_equal(result, "test", "string.slice test between spaces")
    
    result = string.slice("\ttest\t", 2, 5)
    assert_equal(result, "test", "string.slice test between tabs")
    
    result = string.slice("\ntest\n", 2, 5)
    assert_equal(result, "test", "string.slice test between newlines")
    
    result = string.slice("  spaces  everywhere  ", 3, 8)
    assert_equal(result, "spaces", "string.slice spaces word")
    
    result = string.slice("  spaces  everywhere  ", 11, 20)
    assert_equal(result, "everywhere", "string.slice everywhere word")
    
    result = string.slice("one  two   three", 12, 16)
    assert_equal(result, "three", "string.slice three word")
    
    // slice with repeating characters
    result = string.slice("aaaa", 1, 1)
    assert_equal(result, "a", "string.slice a single")
    
    result = string.slice("aaaa", 1, 4)
    assert_equal(result, "aaaa", "string.slice aaaa entire")
    
    result = string.slice("aaaa", 2, 3)
    assert_equal(result, "aa", "string.slice aa middle")
    
    result = string.slice("aaaa", 4, 4)
    assert_equal(result, "a", "string.slice a last")
    
    result = string.slice("aaaa", 5, 6)
    assert_equal(result, "", "string.slice a beyond")
    
    // slice with mixed repeating patterns
    result = string.slice("aabbaabb", 1, 2)
    assert_equal(result, "aa", "string.slice aa in aabbaabb")
    
    result = string.slice("aabbaabb", 3, 4)
    assert_equal(result, "bb", "string.slice bb in aabbaabb")
    
    result = string.slice("aabbaabb", 2, 3)
    assert_equal(result, "ab", "string.slice ab in aabbaabb")
    
    result = string.slice("aabbaabb", 4, 5)
    assert_equal(result, "ba", "string.slice ba in aabbaabb")
    
    result = string.slice("aabbaabb", 1, 4)
    assert_equal(result, "aabb", "string.slice aabb in aabbaabb")
    
    result = string.slice("aabbaabb", 3, 6)
    assert_equal(result, "bbaa", "string.slice bbaa in aabbaabb")
    
    result = string.slice("aabbaabb", 1, 8)
    assert_equal(result, "aabbaabb", "string.slice full aabbaabb")
    
    result = string.slice("aabbaabb", 1, 7)
    assert_equal(result, "aabbaab", "string.slice partial aabbaab")
    
    result = string.slice("aabbaabb", 2, 8)
    assert_equal(result, "abbaabb", "string.slice partial abbaabb")
    
    result = string.slice("aabbaabb", 1, 3)
    assert_equal(result, "aab", "string.slice aab")
    
    result = string.slice("aabbaabb", 2, 4)
    assert_equal(result, "abb", "string.slice abb")
    
    result = string.slice("aabbaabb", 3, 5)
    assert_equal(result, "bba", "string.slice bba")
    
    result = string.slice("aabbaabb", 4, 6)
    assert_equal(result, "baa", "string.slice baa")
    
    // slice alphabet at various positions
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 1, 3)
    assert_equal(result, "abc", "string.slice abc in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 4, 6)
    assert_equal(result, "def", "string.slice def in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 7, 9)
    assert_equal(result, "ghi", "string.slice ghi in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 10, 12)
    assert_equal(result, "jkl", "string.slice jkl in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 13, 15)
    assert_equal(result, "mno", "string.slice mno in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 16, 18)
    assert_equal(result, "pqr", "string.slice pqr in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 19, 21)
    assert_equal(result, "stu", "string.slice stu in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 22, 24)
    assert_equal(result, "vwx", "string.slice vwx in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 25, 26)
    assert_equal(result, "yz", "string.slice yz in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 26, 26)
    assert_equal(result, "z", "string.slice z in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 1, 1)
    assert_equal(result, "a", "string.slice a in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 1, 26)
    assert_equal(result, "abcdefghijklmnopqrstuvwxyz", "string.slice full alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 24, 26)
    assert_equal(result, "xyz", "string.slice xyz in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 23, 26)
    assert_equal(result, "wxyz", "string.slice wxyz in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 22, 26)
    assert_equal(result, "vwxyz", "string.slice vwxyz in alphabet")
    
    result = string.slice("abcdefghijklmnopqrstuvwxyz", 13, 13)
    assert_equal(result, "m", "string.slice m at middle")
    
    // slice with punctuation and symbols
    result = string.slice("!!!", 1, 1)
    assert_equal(result, "!", "string.slice exclamation single")
    
    result = string.slice("!!!", 1, 2)
    assert_equal(result, "!!", "string.slice exclamation double")
    
    result = string.slice("!!!", 1, 3)
    assert_equal(result, "!!!", "string.slice exclamation triple")
    
    result = string.slice("### ### ###", 1, 3)
    assert_equal(result, "###", "string.slice hash triple")
    
    result = string.slice("### ### ###", 5, 7)
    assert_equal(result, "###", "string.slice hash triple middle")
    
    result = string.slice("a$$b$$c", 2, 3)
    assert_equal(result, "$$", "string.slice dollar double")
    
    result = string.slice("a$$b$$c", 2, 2)
    assert_equal(result, "$", "string.slice dollar single")
    
    // slice with file paths
    result = string.slice("/home/user/file.txt", 1, 1)
    assert_equal(result, "/", "string.slice path slash first")
    
    result = string.slice("/home/user/file.txt", 7, 10)
    assert_equal(result, "user", "string.slice user in path")
    
    result = string.slice("/home/user/file.txt", 16, 19)
    assert_equal(result, ".txt", "string.slice .txt in path")
    
    result = string.slice("/home/user/file.txt", 1, 19)
    assert_equal(result, "/home/user/file.txt", "string.slice full path")
    
    // slice with URLs
    result = string.slice("https://example.com", 6, 8)
    assert_equal(result, "://", "string.slice protocol separator")
    
    result = string.slice("https://example.com", 9, 15)
    assert_equal(result, "example", "string.slice domain in URL")
    
    result = string.slice("https://example.com", 16, 19)
    assert_equal(result, ".com", "string.slice .com in URL")
    
    result = string.slice("https://example.com", 1, 5)
    assert_equal(result, "https", "string.slice https in URL")
    
    result = string.slice("https://example.com", 1, 19)
    assert_equal(result, "https://example.com", "string.slice full URL")
    
    // slice with consecutive patterns
    result = string.slice("abcabcabc", 1, 3)
    assert_equal(result, "abc", "string.slice first abc")
    
    result = string.slice("abcabcabc", 4, 6)
    assert_equal(result, "abc", "string.slice second abc")
    
    result = string.slice("abcabcabc", 7, 9)
    assert_equal(result, "abc", "string.slice third abc")
    
    result = string.slice("abcabcabc", 2, 3)
    assert_equal(result, "bc", "string.slice first bc")
    
    result = string.slice("abcabcabc", 3, 3)
    assert_equal(result, "c", "string.slice first c")
    
    result = string.slice(",,,", 1, 1)
    assert_equal(result, ",", "string.slice first comma")
    
    result = string.slice(",,,", 1, 2)
    assert_equal(result, ",,", "string.slice double comma")
    
    result = string.slice(",,,", 1, 3)
    assert_equal(result, ",,,", "string.slice all commas")
    
    // slice with words at various positions
    result = string.slice("red green blue red", 1, 3)
    assert_equal(result, "red", "string.slice first red")
    
    result = string.slice("red green blue red", 5, 9)
    assert_equal(result, "green", "string.slice green")
    
    result = string.slice("red green blue red", 11, 14)
    assert_equal(result, "blue", "string.slice blue")
    
    result = string.slice("red green blue red", 16, 18)
    assert_equal(result, "red", "string.slice last red")
    
    result = string.slice("one one one", 1, 3)
    assert_equal(result, "one", "string.slice first one")
    
    result = string.slice("one one one", 5, 7)
    assert_equal(result, "one", "string.slice second one")
    
    result = string.slice("one one one", 9, 11)
    assert_equal(result, "one", "string.slice third one")
    
    // slice with partial word boundaries
    result = string.slice("hello world", 1, 7)
    assert_equal(result, "hello w", "string.slice partial hello w")
    
    result = string.slice("hello world", 5, 11)
    assert_equal(result, "o world", "string.slice partial o world")
    
    result = string.slice("hello world", 4, 8)
    assert_equal(result, "lo wo", "string.slice partial lo wo")
    
    result = string.slice("hello world", 1, 11)
    assert_equal(result, "hello world", "string.slice full match")
    
    result = string.slice("hello world", 1, 10)
    assert_equal(result, "hello worl", "string.slice partial hello worl")
    
    result = string.slice("hello world", 2, 10)
    assert_equal(result, "ello worl", "string.slice partial ello worl")
    
    result = string.slice("hello world", 5, 7)
    assert_equal(result, "o w", "string.slice partial o w")
    
    result = string.slice("hello world", 6, 6)
    assert_equal(result, " ", "string.slice space")
    
    result = string.slice("hello world", 7, 11)
    assert_equal(result, "world", "string.slice world")
    
    result = string.slice("hello world", 11, 11)
    assert_equal(result, "d", "string.slice last char d")
    
    // slice with zero and negative boundaries
    result = string.slice("test", 0, 0)
    assert_equal(result, "", "string.slice zero zero")
    
    result = string.slice("test", 0, 1)
    assert_equal(result, "t", "string.slice zero to one")
    
    result = string.slice("test", -0, 2)
    assert_equal(result, "te", "string.slice negative zero to two")
    
    result = string.slice("test", -100, 100)
    assert_equal(result, "test", "string.slice huge negative to huge positive")
    
    result = string.slice("test", 100, 200)
    assert_equal(result, "", "string.slice huge positive range")
    
    result = string.slice("test", -100, -50)
    assert_equal(result, "test", "string.slice huge negative range")
    
    // slice with single character strings
    result = string.slice("a", 1, 1)
    assert_equal(result, "a", "string.slice single char string")
    
    result = string.slice("a", 0, 1)
    assert_equal(result, "a", "string.slice single char zero start")
    
    result = string.slice("a", 2, 2)
    assert_equal(result, "", "string.slice single char beyond")
    
    result = string.slice("a", 1, 10)
    assert_equal(result, "a", "string.slice single char to beyond")
    
    result = string.slice("a", -1, 1)
    assert_equal(result, "a", "string.slice single char negative start")
    
    // slice with two character strings
    result = string.slice("ab", 1, 1)
    assert_equal(result, "a", "string.slice two chars first")
    
    result = string.slice("ab", 2, 2)
    assert_equal(result, "b", "string.slice two chars second")
    
    result = string.slice("ab", 1, 2)
    assert_equal(result, "ab", "string.slice two chars both")
    
    result = string.slice("ab", 0, 1)
    assert_equal(result, "a", "string.slice two chars zero start")
    
    result = string.slice("ab", 2, 5)
    assert_equal(result, "b", "string.slice two chars last to beyond")
    
    // slice with three character strings
    result = string.slice("abc", 1, 1)
    assert_equal(result, "a", "string.slice three chars first")
    
    result = string.slice("abc", 2, 2)
    assert_equal(result, "b", "string.slice three chars middle")
    
    result = string.slice("abc", 3, 3)
    assert_equal(result, "c", "string.slice three chars last")
    
    result = string.slice("abc", 1, 3)
    assert_equal(result, "abc", "string.slice three chars all")
    
    result = string.slice("abc", 2, 3)
    assert_equal(result, "bc", "string.slice three chars bc")
    
    result = string.slice("abc", 1, 2)
    assert_equal(result, "ab", "string.slice three chars ab")
    
    // slice with long strings
    result = string.slice("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 1, 25)
    assert_equal(result, "aaaaaaaaaaaaaaaaaaaaaaaaa", "string.slice long string first 25")
    
    result = string.slice("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 26, 50)
    assert_equal(result, "aaaaaaaaaaaaaaaaaaaaaaaaa", "string.slice long string second 25")
    
    result = string.slice("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 1, 50)
    assert_equal(result, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "string.slice long string entire")
    
    result = string.slice("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 51, 60)
    assert_equal(result, "", "string.slice long string beyond")
    
    result = string.slice("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 25)
    assert_equal(result, "aaaaaaaaaaaaaaaaaaaaaaaaa", "string.slice long string zero start")

// test: split string by delimiter into table
function string_split()
    // basic split tests
    parts = string.split("a,b,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split[1]")
        assert_equal(parts[2], "b", "string.split[2]")
        assert_equal(parts[3], "c", "string.split[3]")
    
    // split with different delimiters
    parts = string.split("a;b;c", ";")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split semicolon returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split semicolon[1]")
        assert_equal(parts[2], "b", "string.split semicolon[2]")
        assert_equal(parts[3], "c", "string.split semicolon[3]")
    
    parts = string.split("a:b:c", ":")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split colon returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split colon[1]")
        assert_equal(parts[2], "b", "string.split colon[2]")
        assert_equal(parts[3], "c", "string.split colon[3]")
    
    parts = string.split("a|b|c", "|")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split pipe returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split pipe[1]")
        assert_equal(parts[2], "b", "string.split pipe[2]")
        assert_equal(parts[3], "c", "string.split pipe[3]")
    
    // split with space delimiter
    parts = string.split("hello world apex", " ")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split space returns table")
    if is_table == true
        assert_equal(parts[1], "hello", "string.split space[1]")
        assert_equal(parts[2], "world", "string.split space[2]")
        assert_equal(parts[3], "apex", "string.split space[3]")
    
    // split with multi-character delimiter
    parts = string.split("a---b---c", "---")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split multi-char delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split multi-char[1]")
        assert_equal(parts[2], "b", "string.split multi-char[2]")
        assert_equal(parts[3], "c", "string.split multi-char[3]")
    
    // split with special characters
    parts = string.split("a.b.c.d", ".")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split dot returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split dot[1]")
        assert_equal(parts[2], "b", "string.split dot[2]")
        assert_equal(parts[3], "c", "string.split dot[3]")
        assert_equal(parts[4], "d", "string.split dot[4]")
    
    parts = string.split("path\\to\\file", "\\")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split backslash returns table")
    if is_table == true
        assert_equal(parts[1], "path", "string.split backslash[1]")
        assert_equal(parts[2], "to", "string.split backslash[2]")
        assert_equal(parts[3], "file", "string.split backslash[3]")
    
    // split with newline
    parts = string.split("line1\nline2\nline3", "\n")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split newline returns table")
    if is_table == true
        assert_equal(parts[1], "line1", "string.split newline[1]")
        assert_equal(parts[2], "line2", "string.split newline[2]")
        assert_equal(parts[3], "line3", "string.split newline[3]")
    
    // split with tab
    parts = string.split("col1\tcol2\tcol3", "\t")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split tab returns table")
    if is_table == true
        assert_equal(parts[1], "col1", "string.split tab[1]")
        assert_equal(parts[2], "col2", "string.split tab[2]")
        assert_equal(parts[3], "col3", "string.split tab[3]")
    
    // split with no delimiter in string
    parts = string.split("nodellimiter", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split no delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "nodellimiter", "string.split no delimiter[1]")
    
    // split with multiple delimiters
    parts = string.split("a,,b,,c", ",,")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split double delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split double delimiter[1]")
        assert_equal(parts[2], "b", "string.split double delimiter[2]")
        assert_equal(parts[3], "c", "string.split double delimiter[3]")
    
    // split with consecutive delimiters (single char)
    parts = string.split("a,,b,,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split consecutive commas returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split consecutive commas[1]")
        assert_equal(parts[2], "", "string.split consecutive commas[2]")
        assert_equal(parts[3], "b", "string.split consecutive commas[3]")
        assert_equal(parts[4], "", "string.split consecutive commas[4]")
        assert_equal(parts[5], "c", "string.split consecutive commas[5]")
    
    // split with leading delimiter
    parts = string.split(",a,b,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split leading delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "", "string.split leading delimiter[1]")
        assert_equal(parts[2], "a", "string.split leading delimiter[2]")
        assert_equal(parts[3], "b", "string.split leading delimiter[3]")
        assert_equal(parts[4], "c", "string.split leading delimiter[4]")
    
    // split with trailing delimiter
    parts = string.split("a,b,c,", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split trailing delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split trailing delimiter[1]")
        assert_equal(parts[2], "b", "string.split trailing delimiter[2]")
        assert_equal(parts[3], "c", "string.split trailing delimiter[3]")
        assert_equal(parts[4], "", "string.split trailing delimiter[4]")
    
    // split with leading and trailing delimiters
    parts = string.split(",a,b,c,", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split both delimiters returns table")
    if is_table == true
        assert_equal(parts[1], "", "string.split both delimiters[1]")
        assert_equal(parts[2], "a", "string.split both delimiters[2]")
        assert_equal(parts[3], "b", "string.split both delimiters[3]")
        assert_equal(parts[4], "c", "string.split both delimiters[4]")
        assert_equal(parts[5], "", "string.split both delimiters[5]")
    
    // split empty string
    parts = string.split("", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split empty string returns table")
    if is_table == true
        assert_equal(parts[1], "", "string.split empty string[1]")
    
    // split string with only delimiter
    parts = string.split(",", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split only delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "", "string.split only delimiter[1]")
        assert_equal(parts[2], "", "string.split only delimiter[2]")
    
    // split with multiple characters
    parts = string.split("red,green,blue,yellow,purple", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split colors returns table")
    if is_table == true
        assert_equal(parts[1], "red", "string.split colors[1]")
        assert_equal(parts[2], "green", "string.split colors[2]")
        assert_equal(parts[3], "blue", "string.split colors[3]")
        assert_equal(parts[4], "yellow", "string.split colors[4]")
        assert_equal(parts[5], "purple", "string.split colors[5]")
    
    // split numbers
    parts = string.split("1,2,3,4,5,6,7,8,9,10", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split numbers returns table")
    if is_table == true
        assert_equal(parts[1], "1", "string.split numbers[1]")
        assert_equal(parts[2], "2", "string.split numbers[2]")
        assert_equal(parts[3], "3", "string.split numbers[3]")
        assert_equal(parts[4], "4", "string.split numbers[4]")
        assert_equal(parts[5], "5", "string.split numbers[5]")
        assert_equal(parts[6], "6", "string.split numbers[6]")
        assert_equal(parts[7], "7", "string.split numbers[7]")
        assert_equal(parts[8], "8", "string.split numbers[8]")
        assert_equal(parts[9], "9", "string.split numbers[9]")
        assert_equal(parts[10], "10", "string.split numbers[10]")
    
    // split unicode
    parts = string.split("你好,世界,朋友", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split chinese returns table")
    if is_table == true
        assert_equal(parts[1], "你好", "string.split chinese[1]")
        assert_equal(parts[2], "世界", "string.split chinese[2]")
        assert_equal(parts[3], "朋友", "string.split chinese[3]")
    
    parts = string.split("Привет,мир,друг", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split russian returns table")
    if is_table == true
        assert_equal(parts[1], "Привет", "string.split russian[1]")
        assert_equal(parts[2], "мир", "string.split russian[2]")
        assert_equal(parts[3], "друг", "string.split russian[3]")
    
    parts = string.split("مرحبا,عالم,صديق", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split arabic returns table")
    if is_table == true
        assert_equal(parts[1], "مرحبا", "string.split arabic[1]")
        assert_equal(parts[2], "عالم", "string.split arabic[2]")
        assert_equal(parts[3], "صديق", "string.split arabic[3]")
    
    // split emoji
    parts = string.split("😊,🌟,😀", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split emoji returns table")
    if is_table == true
        assert_equal(parts[1], "😊", "string.split emoji[1]")
        assert_equal(parts[2], "🌟", "string.split emoji[2]")
        assert_equal(parts[3], "😀", "string.split emoji[3]")
    
    // split mixed content
    parts = string.split("word1 word2 word3 word4", " ")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split words returns table")
    if is_table == true
        assert_equal(parts[1], "word1", "string.split words[1]")
        assert_equal(parts[2], "word2", "string.split words[2]")
        assert_equal(parts[3], "word3", "string.split words[3]")
        assert_equal(parts[4], "word4", "string.split words[4]")
    
    // split with single character string
    parts = string.split("a", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split single char returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split single char[1]")
    
    // split with long delimiter
    parts = string.split("part1###part2###part3", "###")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split long delimiter returns table")
    if is_table == true
        assert_equal(parts[1], "part1", "string.split long delimiter[1]")
        assert_equal(parts[2], "part2", "string.split long delimiter[2]")
        assert_equal(parts[3], "part3", "string.split long delimiter[3]")
    
    // split with whitespace variations
    parts = string.split("one two  three   four", " ")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split spaces returns table")
    if is_table == true
        assert_equal(parts[1], "one", "string.split spaces[1]")
        assert_equal(parts[2], "two", "string.split spaces[2]")
        assert_equal(parts[3], "", "string.split spaces[3]")
        assert_equal(parts[4], "three", "string.split spaces[4]")
        assert_equal(parts[5], "", "string.split spaces[5]")
        assert_equal(parts[6], "", "string.split spaces[6]")
        assert_equal(parts[7], "four", "string.split spaces[7]")
    
    // split file path
    parts = string.split("/home/user/file.txt", "/")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split path returns table")
    if is_table == true
        assert_equal(parts[1], "", "string.split path[1]")
        assert_equal(parts[2], "home", "string.split path[2]")
        assert_equal(parts[3], "user", "string.split path[3]")
        assert_equal(parts[4], "file.txt", "string.split path[4]")
    
    // split URL
    parts = string.split("https://example.com/path/to/page", "/")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split URL returns table")
    if is_table == true
        assert_equal(parts[1], "https:", "string.split URL[1]")
        assert_equal(parts[2], "", "string.split URL[2]")
        assert_equal(parts[3], "example.com", "string.split URL[3]")
        assert_equal(parts[4], "path", "string.split URL[4]")
        assert_equal(parts[5], "to", "string.split URL[5]")
        assert_equal(parts[6], "page", "string.split URL[6]")
    
    // split with punctuation
    parts = string.split("sentence one. sentence two. sentence three.", ".")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split sentences returns table")
    if is_table == true
        assert_equal(parts[1], "sentence one", "string.split sentences[1]")
        assert_equal(parts[2], " sentence two", "string.split sentences[2]")
        assert_equal(parts[3], " sentence three", "string.split sentences[3]")
        assert_equal(parts[4], "", "string.split sentences[4]")
    
    // split with mixed delimiters
    parts = string.split("key=value&name=test&id=123", "&")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split query params returns table")
    if is_table == true
        assert_equal(parts[1], "key=value", "string.split query params[1]")
        assert_equal(parts[2], "name=test", "string.split query params[2]")
        assert_equal(parts[3], "id=123", "string.split query params[3]")
    
    // split very long string
    parts = string.split("aaaaaaaaaa,bbbbbbbbbb,cccccccccc,dddddddddd,eeeeeeeeee", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split long strings returns table")
    if is_table == true
        assert_equal(parts[1], "aaaaaaaaaa", "string.split long strings[1]")
        assert_equal(parts[2], "bbbbbbbbbb", "string.split long strings[2]")
        assert_equal(parts[3], "cccccccccc", "string.split long strings[3]")
        assert_equal(parts[4], "dddddddddd", "string.split long strings[4]")
        assert_equal(parts[5], "eeeeeeeeee", "string.split long strings[5]")

// test: join table elements into string with separator
function string_join()
    parts = string.split("a,b,c", ",")
    result = string.join(parts, "-")
    assert_equal(result, "a-b-c", "string.join with dash")
    
    result = string.join(parts, "")
    assert_equal(result, "abc", "string.join empty separator")

// test: trim whitespace from both ends
function string_trim()
    // basic trim tests
    result = string.trim("  hello  ")
    assert_equal(result, "hello", "string.trim spaces")
    
    result = string.trim("\t\nhello\t\n")
    assert_equal(result, "hello", "string.trim tabs newlines")
    
    result = string.trim("hello")
    assert_equal(result, "hello", "string.trim no change")
    
    // trim with only spaces
    result = string.trim("   spaces   ")
    assert_equal(result, "spaces", "string.trim multiple spaces")
    
    result = string.trim(" single space ")
    assert_equal(result, "single space", "string.trim single spaces")
    
    result = string.trim("  double  spaces  ")
    assert_equal(result, "double  spaces", "string.trim internal spaces preserved")
    
    // trim with tabs
    result = string.trim("\ttabbed\t")
    assert_equal(result, "tabbed", "string.trim tabs")
    
    result = string.trim("\t\tmultiple tabs\t\t")
    assert_equal(result, "multiple tabs", "string.trim multiple tabs")
    
    result = string.trim("\t mixed \t")
    assert_equal(result, "mixed", "string.trim mixed tabs spaces")
    
    // trim with newlines
    result = string.trim("\nnewline\n")
    assert_equal(result, "newline", "string.trim newlines")
    
    result = string.trim("\n\nmultiple newlines\n\n")
    assert_equal(result, "multiple newlines", "string.trim multiple newlines")
    
    result = string.trim("\r\ncarriage return\r\n")
    assert_equal(result, "carriage return", "string.trim carriage return")
    
    // trim with mixed whitespace
    result = string.trim(" \t\nmixed whitespace\n\t ")
    assert_equal(result, "mixed whitespace", "string.trim mixed whitespace")
    
    result = string.trim("\t \n \tall types\t \n \t")
    assert_equal(result, "all types", "string.trim all whitespace types")
    
    // trim with only whitespace
    result = string.trim("   ")
    assert_equal(result, "", "string.trim only spaces")
    
    result = string.trim("\t\t")
    assert_equal(result, "", "string.trim only tabs")
    
    result = string.trim("\n\n")
    assert_equal(result, "", "string.trim only newlines")
    
    result = string.trim(" \t\n ")
    assert_equal(result, "", "string.trim only mixed whitespace")
    
    // trim empty string
    result = string.trim("")
    assert_equal(result, "", "string.trim empty string")
    
    // trim single character
    result = string.trim("a")
    assert_equal(result, "a", "string.trim single char")
    
    result = string.trim(" a")
    assert_equal(result, "a", "string.trim space before char")
    
    result = string.trim("a ")
    assert_equal(result, "a", "string.trim space after char")
    
    result = string.trim(" a ")
    assert_equal(result, "a", "string.trim spaces around char")
    
    // trim with special characters
    result = string.trim("  !@#$%  ")
    assert_equal(result, "!@#$%", "string.trim special chars")
    
    result = string.trim("  ...  ")
    assert_equal(result, "...", "string.trim dots")
    
    result = string.trim("  ,,,  ")
    assert_equal(result, ",,,", "string.trim commas")
    
    // trim with punctuation
    result = string.trim("  Hello, World!  ")
    assert_equal(result, "Hello, World!", "string.trim punctuation")
    
    result = string.trim("  What's up?  ")
    assert_equal(result, "What's up?", "string.trim apostrophe")
    
    result = string.trim("  (parentheses)  ")
    assert_equal(result, "(parentheses)", "string.trim parentheses")
    
    // trim with numbers
    result = string.trim("  12345  ")
    assert_equal(result, "12345", "string.trim numbers")
    
    result = string.trim("  1-2-3-4-5  ")
    assert_equal(result, "1-2-3-4-5", "string.trim numbers with dashes")
    
    result = string.trim("  100%  ")
    assert_equal(result, "100%", "string.trim percentage")
    
    result = string.trim("  $100.50  ")
    assert_equal(result, "$100.50", "string.trim currency")
    
    // trim with Unicode
    result = string.trim("  你好  ")
    assert_equal(result, "你好", "string.trim chinese")
    
    result = string.trim("  你好世界  ")
    assert_equal(result, "你好世界", "string.trim chinese multi")
    
    result = string.trim("  Привет  ")
    assert_equal(result, "Привет", "string.trim russian")
    
    result = string.trim("  Привет Мир  ")
    assert_equal(result, "Привет Мир", "string.trim russian multi")
    
    result = string.trim("  مرحبا  ")
    assert_equal(result, "مرحبا", "string.trim arabic")
    
    result = string.trim("  السلام عليكم  ")
    assert_equal(result, "السلام عليكم", "string.trim arabic multi")
    
    result = string.trim("  こんにちは  ")
    assert_equal(result, "こんにちは", "string.trim japanese")
    
    result = string.trim("  안녕하세요  ")
    assert_equal(result, "안녕하세요", "string.trim korean")
    
    // trim with emoji
    result = string.trim("  😊  ")
    assert_equal(result, "😊", "string.trim emoji")
    
    result = string.trim("  😊🌟😊  ")
    assert_equal(result, "😊🌟😊", "string.trim emoji multi")
    
    result = string.trim("  🎉🎊🎈  ")
    assert_equal(result, "🎉🎊🎈", "string.trim celebration emojis")
    
    // trim with mixed scripts
    result = string.trim("  Hello Привет 你好  ")
    assert_equal(result, "Hello Привет 你好", "string.trim mixed scripts")
    
    result = string.trim("  123 abc 你好  ")
    assert_equal(result, "123 abc 你好", "string.trim mixed alphanumeric")
    
    // trim with file paths and URLs
    result = string.trim("  /home/user/file.txt  ")
    assert_equal(result, "/home/user/file.txt", "string.trim file path")
    
    result = string.trim("  https://example.com  ")
    assert_equal(result, "https://example.com", "string.trim URL")
    
    result = string.trim("  C:\\Users\\test  ")
    assert_equal(result, "C:\\Users\\test", "string.trim windows path")
    
    // trim with email
    result = string.trim("  user@example.com  ")
    assert_equal(result, "user@example.com", "string.trim email")
    
    result = string.trim("  test.user@sub.domain.com  ")
    assert_equal(result, "test.user@sub.domain.com", "string.trim long email")
    
    // trim with code
    result = string.trim("  function test() \{ \}  ")
    assert_equal(result, "function test() \{ \}", "string.trim function")
    
    result = string.trim("  var x = 10;  ")
    assert_equal(result, "var x = 10;", "string.trim variable")
    
    result = string.trim("  if (x > 5) \{ return true; \}  ")
    assert_equal(result, "if (x > 5) \{ return true; \}", "string.trim if statement")
    
    // trim with JSON
    result = string.trim("  \{\"key\": \"value\"\}  ")
    assert_equal(result, "\{\"key\": \"value\"\}", "string.trim JSON")
    
    result = string.trim("  [1, 2, 3]  ")
    assert_equal(result, "[1, 2, 3]", "string.trim array")
    
    // trim with HTML
    result = string.trim("  <div>content</div>  ")
    assert_equal(result, "<div>content</div>", "string.trim HTML")
    
    result = string.trim("  <p class=\"test\">text</p>  ")
    assert_equal(result, "<p class=\"test\">text</p>", "string.trim HTML attrs")
    
    // trim with multiline strings
    result = string.trim("  line1\nline2\nline3  ")
    assert_equal(result, "line1\nline2\nline3", "string.trim multiline")
    
    result = string.trim("\n  first\n  second\n  third  \n")
    assert_equal(result, "first\n  second\n  third", "string.trim preserve internal newlines")
    
    // trim with long strings
    result = string.trim("  aaaaaaaaaaaaaaaaaaaaaaaaaa  ")
    assert_equal(result, "aaaaaaaaaaaaaaaaaaaaaaaaaa", "string.trim long string")
    
    result = string.trim("  abcabcabcabcabcabcabcabcabcabc  ")
    assert_equal(result, "abcabcabcabcabcabcabcabcabcabc", "string.trim repeated pattern")
    
    // trim with repeating characters
    result = string.trim("  aaaa  ")
    assert_equal(result, "aaaa", "string.trim repeating chars")
    
    result = string.trim("  aabbaabb  ")
    assert_equal(result, "aabbaabb", "string.trim mixed repeating")
    
    // trim with leading only
    result = string.trim("  hello")
    assert_equal(result, "hello", "string.trim leading only")
    
    result = string.trim("\t\nhello")
    assert_equal(result, "hello", "string.trim leading tabs newlines")
    
    result = string.trim(" \t\n hello")
    assert_equal(result, "hello", "string.trim leading mixed")
    
    // trim with trailing only
    result = string.trim("hello  ")
    assert_equal(result, "hello", "string.trim trailing only")
    
    result = string.trim("hello\t\n")
    assert_equal(result, "hello", "string.trim trailing tabs newlines")
    
    result = string.trim("hello \t\n ")
    assert_equal(result, "hello", "string.trim trailing mixed")
    
    // trim with asymmetric whitespace
    result = string.trim("   hello ")
    assert_equal(result, "hello", "string.trim asymmetric spaces")
    
    result = string.trim("\t hello \n")
    assert_equal(result, "hello", "string.trim asymmetric mixed")
    
    // trim with many whitespace characters
    result = string.trim("                    hello                    ")
    assert_equal(result, "hello", "string.trim many spaces")
    
    result = string.trim("\t\t\t\t\t\t\t\t hello \t\t\t\t\t\t\t\t")
    assert_equal(result, "hello", "string.trim many tabs")
    
    // trim preserving internal whitespace exactly
    result = string.trim("  hello   world  ")
    assert_equal(result, "hello   world", "string.trim preserve internal spaces")
    
    result = string.trim("  hello\tworld  ")
    assert_equal(result, "hello\tworld", "string.trim preserve internal tab")
    
    result = string.trim("  hello\nworld  ")
    assert_equal(result, "hello\nworld", "string.trim preserve internal newline")
    
    result = string.trim("  hello \t\n world  ")
    assert_equal(result, "hello \t\n world", "string.trim preserve internal mixed")
    
    // trim with strings that are all whitespace
    result = string.trim("                    ")
    assert_equal(result, "", "string.trim all spaces long")
    
    result = string.trim("\t\t\t\t\t\t\t\t\t\t")
    assert_equal(result, "", "string.trim all tabs long")
    
    result = string.trim("\n\n\n\n\n\n\n\n\n\n")
    assert_equal(result, "", "string.trim all newlines long")
    
    result = string.trim(" \t\n \t\n \t\n \t\n ")
    assert_equal(result, "", "string.trim all mixed long")
    
    // trim with whitespace and special characters
    result = string.trim("  \"quoted\"  ")
    assert_equal(result, "\"quoted\"", "string.trim quoted")
    
    result = string.trim("  'single quoted'  ")
    assert_equal(result, "'single quoted'", "string.trim single quoted")
    
    result = string.trim("  \{braces\}  ")
    assert_equal(result, "\{braces\}", "string.trim braces")
    
    result = string.trim("  [brackets]  ")
    assert_equal(result, "[brackets]", "string.trim brackets")
    
    // trim with escaped characters
    result = string.trim("  C:\\Users\\test  ")
    assert_equal(result, "C:\\Users\\test", "string.trim escaped backslashes")
    
    result = string.trim("  \"escaped \\\" quotes\"  ")
    assert_equal(result, "\"escaped \\\" quotes\"", "string.trim escaped quotes")

// test: find substring position
function string_find()
    // basic find tests
    index = string.find("Hello World", "World")
    assert_equal(index, 7, "string.find World")
    
    index = string.find("Hello World", "Hello")
    assert_equal(index, 1, "string.find Hello")
    
    index = string.find("Hello World", "xyz")
    assert_equal(index, -1, "string.find not found")
    
    // find at beginning and end
    index = string.find("start middle end", "start")
    assert_equal(index, 1, "string.find at beginning")
    
    index = string.find("start middle end", "end")
    assert_equal(index, 14, "string.find at end")
    
    // find entire string
    index = string.find("complete", "complete")
    assert_equal(index, 1, "string.find entire string")
    
    // find single character
    index = string.find("hello", "h")
    assert_equal(index, 1, "string.find single char h")
    
    index = string.find("hello", "o")
    assert_equal(index, 5, "string.find single char o")
    
    index = string.find("hello", "l")
    assert_equal(index, 3, "string.find first occurrence l")
    
    // find empty string
    index = string.find("test", "")
    assert_equal(index, 1, "string.find empty pattern")
    
    index = string.find("", "a")
    assert_equal(index, -1, "string.find empty source")
    
    // find with multiple occurrences
    index = string.find("hello hello hello", "hello")
    assert_equal(index, 1, "string.find first hello")
    
    index = string.find("test testing tested", "test")
    assert_equal(index, 1, "string.find first test")
    
    // find substring within word
    index = string.find("cat concatenate category", "cat")
    assert_equal(index, 1, "string.find cat in cat concatenate")
    
    index = string.find("concatenate", "cat")
    assert_equal(index, 4, "string.find cat in concatenate")
    
    // case sensitivity
    index = string.find("Hello hello HELLO", "hello")
    assert_equal(index, 7, "string.find case sensitive hello")
    
    index = string.find("Hello hello HELLO", "HELLO")
    assert_equal(index, 13, "string.find case sensitive HELLO")
    
    index = string.find("Hello hello HELLO", "Hello")
    assert_equal(index, 1, "string.find case sensitive Hello")
    
    // unicode find
    index = string.find("你好世界你好", "你好")
    assert_equal(index, 1, "string.find chinese 你好 first")
    
    index = string.find("你好世界你好", "世界")
    assert_equal(index, 3, "string.find chinese 世界")
    
    index = string.find("Привет мир Привет", "Привет")
    assert_equal(index, 1, "string.find russian Привет first")
    
    index = string.find("Привет мир Привет", "мир")
    assert_equal(index, 8, "string.find russian мир")
    
    index = string.find("مرحبا عالم مرحبا", "مرحبا")
    assert_equal(index, 1, "string.find arabic مرحبا first")
    
    index = string.find("مرحبا عالم مرحبا", "عالم")
    assert_equal(index, 7, "string.find arabic عالم")
    
    // mixed scripts find
    index = string.find("Hello Привет 你好", "Привет")
    assert_equal(index, 7, "string.find mixed Привет")
    
    index = string.find("Hello Привет 你好", "你好")
    assert_equal(index, 14, "string.find mixed 你好")
    
    index = string.find("Hello Привет 你好", "Hello")
    assert_equal(index, 1, "string.find mixed Hello")
    
    // emoji find
    index = string.find("😊🌟😊", "😊")
    assert_equal(index, 1, "string.find emoji 😊 first")
    
    index = string.find("😊🌟😊", "🌟")
    assert_equal(index, 2, "string.find emoji 🌟")
    
    index = string.find("🌟😊🌟😊", "😊")
    assert_equal(index, 2, "string.find emoji 😊 in 🌟😊🌟😊")
    
    // special characters
    index = string.find("a.b.c.d", ".")
    assert_equal(index, 2, "string.find dot first")
    
    index = string.find("a.b.c.d", ".d")
    assert_equal(index, 6, "string.find .d")
    
    index = string.find("path\\to\\file", "\\")
    assert_equal(index, 5, "string.find backslash first")
    
    index = string.find("path\\to\\file", "file")
    assert_equal(index, 9, "string.find file after backslash")
    
    index = string.find("line1\nline2\nline3", "\n")
    assert_equal(index, 6, "string.find newline first")
    
    index = string.find("line1\nline2\nline3", "line2")
    assert_equal(index, 7, "string.find line2")
    
    index = string.find("tab\tseparated\tvalues", "\t")
    assert_equal(index, 4, "string.find tab first")
    
    index = string.find("tab\tseparated\tvalues", "values")
    assert_equal(index, 15, "string.find values")
    
    // numbers and mixed content
    index = string.find("1-2-3-4-5", "-")
    assert_equal(index, 2, "string.find dash first")
    
    index = string.find("1-2-3-4-5", "3")
    assert_equal(index, 5, "string.find digit 3")
    
    index = string.find("a1b2c3", "1")
    assert_equal(index, 2, "string.find digit 1 in a1b2c3")
    
    index = string.find("a1b2c3", "b2")
    assert_equal(index, 3, "string.find b2")
    
    index = string.find("version 1.0 version 2.0", "version")
    assert_equal(index, 1, "string.find version first")
    
    index = string.find("version 1.0 version 2.0", "2.0")
    assert_equal(index, 21, "string.find 2.0")
    
    // find with spaces
    index = string.find("  spaces  everywhere  ", "spaces")
    assert_equal(index, 3, "string.find spaces")
    
    index = string.find("  spaces  everywhere  ", "everywhere")
    assert_equal(index, 11, "string.find everywhere")
    
    index = string.find("one  two   three", "  ")
    assert_equal(index, 4, "string.find double space")
    
    index = string.find("one  two   three", "three")
    assert_equal(index, 12, "string.find three")
    
    // find patterns as substrings of other patterns
    index = string.find("cat concatenate category", "con")
    assert_equal(index, 5, "string.find con in concatenate")
    
    index = string.find("test testing tested", "testi")
    assert_equal(index, 6, "string.find testi in testing")
    
    index = string.find("test testing tested", "tested")
    assert_equal(index, 14, "string.find tested")
    
    // find with punctuation and symbols
    index = string.find("!!!", "!")
    assert_equal(index, 1, "string.find ! first")
    
    index = string.find("!!!", "!!")
    assert_equal(index, 1, "string.find !! first")
    
    index = string.find("### ### ###", "###")
    assert_equal(index, 1, "string.find ### first")
    
    index = string.find("### ### ###", "#")
    assert_equal(index, 1, "string.find # first")
    
    index = string.find("a$$b$$c", "$$")
    assert_equal(index, 2, "string.find $$ first")
    
    index = string.find("a$$b$$c", "$")
    assert_equal(index, 2, "string.find $ first")
    
    // find with pattern longer than source
    index = string.find("short", "very long pattern")
    assert_equal(index, -1, "string.find pattern longer than source")
    
    index = string.find("a", "abc")
    assert_equal(index, -1, "string.find pattern too long")
    
    // find with overlapping patterns (first occurrence only)
    index = string.find("aaaa", "aa")
    assert_equal(index, 1, "string.find first aa in aaaa")
    
    index = string.find("aaaaa", "aa")
    assert_equal(index, 1, "string.find first aa in aaaaa")
    
    index = string.find("aaaa", "aaa")
    assert_equal(index, 1, "string.find first aaa in aaaa")
    
    // find with pattern not in source
    index = string.find("hello world", "xyz")
    assert_equal(index, -1, "string.find xyz not found")
    
    index = string.find("hello world", "worlds")
    assert_equal(index, -1, "string.find worlds not found")
    
    index = string.find("hello world", "Hello")
    assert_equal(index, -1, "string.find case mismatch not found")
    
    index = string.find("hello world", "hello world!")
    assert_equal(index, -1, "string.find longer pattern not found")
    
    index = string.find("hello world", "hello  world")
    assert_equal(index, -1, "string.find double space not found")
    
    index = string.find("12345", "6")
    assert_equal(index, -1, "string.find digit 6 not found")
    
    index = string.find("12345", "123456")
    assert_equal(index, -1, "string.find longer number not found")
    
    index = string.find("!@#$%", "^")
    assert_equal(index, -1, "string.find caret not found")
    
    index = string.find("!@#$%", "&")
    assert_equal(index, -1, "string.find ampersand not found")
    
    // find with file paths
    index = string.find("/home/user/file.txt", "/")
    assert_equal(index, 1, "string.find path slash first")
    
    index = string.find("/home/user/file.txt", "user")
    assert_equal(index, 7, "string.find user in path")
    
    index = string.find("/home/user/file.txt", ".txt")
    assert_equal(index, 16, "string.find .txt in path")
    
    // find with URLs
    index = string.find("https://example.com", "://")
    assert_equal(index, 6, "string.find protocol separator")
    
    index = string.find("https://example.com", "example")
    assert_equal(index, 9, "string.find domain in URL")
    
    index = string.find("https://example.com", ".com")
    assert_equal(index, 16, "string.find .com in URL")
    
    index = string.find("https://example.com", "https")
    assert_equal(index, 1, "string.find https in URL")
    
    // find with consecutive patterns
    index = string.find("abcabcabc", "abc")
    assert_equal(index, 1, "string.find first abc")
    
    index = string.find("abcabcabc", "bc")
    assert_equal(index, 2, "string.find first bc")
    
    index = string.find("abcabcabc", "c")
    assert_equal(index, 3, "string.find first c")
    
    index = string.find(",,,", ",")
    assert_equal(index, 1, "string.find first comma")
    
    index = string.find(",,,", ",,")
    assert_equal(index, 1, "string.find double comma")
    
    // find with pattern appearing multiple times
    index = string.find("red green blue red", "red")
    assert_equal(index, 1, "string.find first red")
    
    index = string.find("red green blue red", "green")
    assert_equal(index, 5, "string.find green")
    
    index = string.find("red green blue red", "blue")
    assert_equal(index, 11, "string.find blue")
    
    index = string.find("one one one", "one")
    assert_equal(index, 1, "string.find first one")
    
    // find with whitespace at boundaries
    index = string.find(" test", "test")
    assert_equal(index, 2, "string.find test after space")
    
    index = string.find("test ", "test")
    assert_equal(index, 1, "string.find test before space")
    
    index = string.find(" test ", "test")
    assert_equal(index, 2, "string.find test between spaces")
    
    index = string.find("\ttest\t", "test")
    assert_equal(index, 2, "string.find test between tabs")
    
    index = string.find("\ntest\n", "test")
    assert_equal(index, 2, "string.find test between newlines")
    
    // find with pattern partially matching
    index = string.find("hello world", "hello w")
    assert_equal(index, 1, "string.find partial hello w")
    
    index = string.find("hello world", "o world")
    assert_equal(index, 5, "string.find partial o world")
    
    index = string.find("hello world", "lo wo")
    assert_equal(index, 4, "string.find partial lo wo")
    
    index = string.find("hello world", "hello world")
    assert_equal(index, 1, "string.find full match")
    
    index = string.find("hello world", "hello worl")
    assert_equal(index, 1, "string.find partial hello worl")
    
    index = string.find("hello world", "ello worl")
    assert_equal(index, 2, "string.find partial ello worl")
    
    index = string.find("hello world", "o w")
    assert_equal(index, 5, "string.find partial o w")
    
    index = string.find("hello world", " ")
    assert_equal(index, 6, "string.find space")
    
    index = string.find("hello world", "world")
    assert_equal(index, 7, "string.find world")
    
    index = string.find("hello world", "d")
    assert_equal(index, 11, "string.find last char d")
    
    // find with repeating characters
    index = string.find("aaaa", "a")
    assert_equal(index, 1, "string.find a in aaaa")
    
    index = string.find("aaaa", "aa")
    assert_equal(index, 1, "string.find aa in aaaa")
    
    index = string.find("aaaa", "aaa")
    assert_equal(index, 1, "string.find aaa in aaaa")
    
    index = string.find("aaaa", "aaaa")
    assert_equal(index, 1, "string.find aaaa in aaaa")
    
    index = string.find("aaaa", "aaaaa")
    assert_equal(index, -1, "string.find aaaaa in aaaa not found")
    
    index = string.find("aaaa", "b")
    assert_equal(index, -1, "string.find b in aaaa not found")
    
    // find with mixed repeating patterns
    index = string.find("aabbaabb", "aa")
    assert_equal(index, 1, "string.find aa in aabbaabb")
    
    index = string.find("aabbaabb", "bb")
    assert_equal(index, 3, "string.find bb in aabbaabb")
    
    index = string.find("aabbaabb", "ab")
    assert_equal(index, 2, "string.find ab in aabbaabb")
    
    index = string.find("aabbaabb", "ba")
    assert_equal(index, 4, "string.find ba in aabbaabb")
    
    index = string.find("aabbaabb", "aabb")
    assert_equal(index, 1, "string.find aabb in aabbaabb")
    
    index = string.find("aabbaabb", "bbaa")
    assert_equal(index, 3, "string.find bbaa in aabbaabb")
    
    index = string.find("aabbaabb", "aabbaabb")
    assert_equal(index, 1, "string.find full aabbaabb")
    
    index = string.find("aabbaabb", "aabbaab")
    assert_equal(index, 1, "string.find partial aabbaab")
    
    index = string.find("aabbaabb", "abbaabb")
    assert_equal(index, 2, "string.find partial abbaabb")
    
    index = string.find("aabbaabb", "aab")
    assert_equal(index, 1, "string.find aab in aabbaabb")
    
    index = string.find("aabbaabb", "abb")
    assert_equal(index, 2, "string.find abb in aabbaabb")
    
    index = string.find("aabbaabb", "bba")
    assert_equal(index, 3, "string.find bba in aabbaabb")
    
    index = string.find("aabbaabb", "baa")
    assert_equal(index, 4, "string.find baa in aabbaabb")
    
    index = string.find("aabbaabb", "a")
    assert_equal(index, 1, "string.find a in aabbaabb")
    
    index = string.find("aabbaabb", "b")
    assert_equal(index, 3, "string.find b in aabbaabb")
    
    // find with pattern at various positions in longer string
    index = string.find("abcdefghijklmnopqrstuvwxyz", "abc")
    assert_equal(index, 1, "string.find abc in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "def")
    assert_equal(index, 4, "string.find def in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "ghi")
    assert_equal(index, 7, "string.find ghi in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "jkl")
    assert_equal(index, 10, "string.find jkl in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "mno")
    assert_equal(index, 13, "string.find mno in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "pqr")
    assert_equal(index, 16, "string.find pqr in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "stu")
    assert_equal(index, 19, "string.find stu in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "vwx")
    assert_equal(index, 22, "string.find vwx in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "yz")
    assert_equal(index, 25, "string.find yz in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "z")
    assert_equal(index, 26, "string.find z in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "a")
    assert_equal(index, 1, "string.find a in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz")
    assert_equal(index, 1, "string.find full alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "xyz")
    assert_equal(index, 24, "string.find xyz in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "wxyz")
    assert_equal(index, 23, "string.find wxyz in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "vwxyz")
    assert_equal(index, 22, "string.find vwxyz in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
    assert_equal(index, -1, "string.find uppercase alphabet not found")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "1234567890")
    assert_equal(index, -1, "string.find numbers not found in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "!@#$%^&*()")
    assert_equal(index, -1, "string.find symbols not found in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", " ")
    assert_equal(index, -1, "string.find space not found in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "")
    assert_equal(index, 1, "string.find empty string in alphabet")
    
    index = string.find("abcdefghijklmnopqrstuvwxyz", "m")
    assert_equal(index, 13, "string.find m at middle of alphabet")

// test: replace substring with another
function string_replace()
    // basic replacement tests
    result = string.replace("Hello World", "World", "Apex")
    assert_equal(result, "Hello Apex", "string.replace World->Apex")
    
    result = string.replace("Hello World", "xyz", "abc")
    assert_equal(result, "Hello World", "string.replace not found")
    
    // replace all occurrences (multiple matches)
    result = string.replace("aaa", "a", "b")
    assert_equal(result, "bbb", "string.replace all a->b")
    
    result = string.replace("hello hello hello", "hello", "bye")
    assert_equal(result, "bye bye bye", "string.replace all hello->bye")
    
    result = string.replace("test test test", "test", "exam")
    assert_equal(result, "exam exam exam", "string.replace all test->exam")
    
    // replace with longer string
    result = string.replace("a b c", " ", "---")
    assert_equal(result, "a---b---c", "string.replace space->dashes")
    
    result = string.replace("one two three", " ", "_X_")
    assert_equal(result, "one_X_two_X_three", "string.replace space->_X_")
    
    // replace with shorter string
    result = string.replace("aaa---bbb---ccc", "---", "-")
    assert_equal(result, "aaa-bbb-ccc", "string.replace ---->-")
    
    result = string.replace("hello world hello world", "hello ", "")
    assert_equal(result, "world world", "string.replace hello->empty")
    
    // replace with empty string (deletion)
    result = string.replace("remove spaces here", " ", "")
    assert_equal(result, "removespaceshere", "string.replace spaces->empty")
    
    result = string.replace("abcabcabc", "b", "")
    assert_equal(result, "acacac", "string.replace b->empty")
    
    // overlapping patterns (non-overlapping replacement)
    result = string.replace("aaaa", "aa", "b")
    assert_equal(result, "bb", "string.replace overlapping aa->b")
    
    result = string.replace("aaaaa", "aa", "a")
    assert_equal(result, "aaa", "string.replace overlapping aa->a")
    
    // replace at beginning and end
    result = string.replace("start middle end", "start", "begin")
    assert_equal(result, "begin middle end", "string.replace at beginning")
    
    result = string.replace("start middle end", "end", "finish")
    assert_equal(result, "start middle finish", "string.replace at end")
    
    result = string.replace("xxx test", "xxx", "yyy")
    assert_equal(result, "yyy test", "string.replace prefix xxx->yyy")
    
    result = string.replace("test xxx", "xxx", "yyy")
    assert_equal(result, "test yyy", "string.replace suffix xxx->yyy")
    
    // replace entire string
    result = string.replace("complete", "complete", "done")
    assert_equal(result, "done", "string.replace entire string")
    
    result = string.replace("exact", "exact", "")
    assert_equal(result, "", "string.replace entire->empty")
    
    // case sensitivity
    result = string.replace("Hello hello HELLO", "hello", "bye")
    assert_equal(result, "Hello bye HELLO", "string.replace case sensitive")
    
    result = string.replace("ABC abc AbC", "abc", "XYZ")
    assert_equal(result, "ABC XYZ AbC", "string.replace case sensitive 2")
    
    // unicode replacement
    result = string.replace("你好世界你好", "你好", "再见")
    assert_equal(result, "再见世界再见", "string.replace chinese 你好->再见")
    
    result = string.replace("Привет мир Привет", "Привет", "Здравствуй")
    assert_equal(result, "Здравствуй мир Здравствуй", "string.replace russian Привет->Здравствуй")
    
    result = string.replace("مرحبا عالم مرحبا", "مرحبا", "أهلا")
    assert_equal(result, "أهلا عالم أهلا", "string.replace arabic مرحبا->أهلا")
    
    // mixed scripts replacement
    result = string.replace("Hello 你好 World 你好", "你好", "Привет")
    assert_equal(result, "Hello Привет World Привет", "string.replace mixed 你好->Привет")
    
    // emoji replacement
    result = string.replace("😊🌟😊", "😊", "😀")
    assert_equal(result, "😀🌟😀", "string.replace emoji 😊->😀")
    
    result = string.replace("🌟😊🌟😊", "😊", "")
    assert_equal(result, "🌟🌟", "string.replace emoji 😊->empty")
    
    // special characters
    result = string.replace("a.b.c.d", ".", "/")
    assert_equal(result, "a/b/c/d", "string.replace dots->slashes")
    
    result = string.replace("path\\to\\file", "\\", "/")
    assert_equal(result, "path/to/file", "string.replace backslash->slash")
    
    result = string.replace("line1\nline2\nline3", "\n", "|")
    assert_equal(result, "line1|line2|line3", "string.replace newlines->pipe")
    
    result = string.replace("tab\tseparated\tvalues", "\t", ",")
    assert_equal(result, "tab,separated,values", "string.replace tabs->commas")
    
    // numbers and mixed content
    result = string.replace("1-2-3-4-5", "-", "+")
    assert_equal(result, "1+2+3+4+5", "string.replace dashes->plus")
    
    result = string.replace("a1b2c3", "1", "one")
    assert_equal(result, "aoneb2c3", "string.replace digits->words")
    
    result = string.replace("version 1.0 version 2.0", "version", "v")
    assert_equal(result, "v 1.0 v 2.0", "string.replace version->v")
    
    // empty string edge cases
    result = string.replace("", "a", "b")
    assert_equal(result, "", "string.replace empty source")
    
    result = string.replace("test", "", "x")
    assert_equal(result, "test", "string.replace empty pattern")
    
    result = string.replace("test", "t", "")
    assert_equal(result, "es", "string.replace t->empty")
    
    // single character replacements
    result = string.replace("aaaa", "a", "b")
    assert_equal(result, "bbbb", "string.replace a->b")
    
    result = string.replace("aaaa", "a", "bb")
    assert_equal(result, "bbbbbbbb", "string.replace a->bb")
    
    result = string.replace("aaaa", "aa", "b")
    assert_equal(result, "bb", "string.replace aa->b")
    
    // multiple different patterns
    result = string.replace("red green blue red", "red", "yellow")
    assert_equal(result, "yellow green blue yellow", "string.replace red->yellow")
    
    result = string.replace("one one one", "one", "1")
    assert_equal(result, "1 1 1", "string.replace one->1")
    
    // consecutive patterns
    result = string.replace("abcabcabc", "abc", "")
    assert_equal(result, "", "string.replace abc->empty")
    
    result = string.replace(",,,", ",", ";")
    assert_equal(result, ";;;", "string.replace commas->semicolons")
    
    // patterns appearing as substrings of other patterns
    result = string.replace("cat concatenate category", "cat", "dog")
    assert_equal(result, "dog condogenate dogegory", "string.replace cat->dog")
    
    result = string.replace("test testing tested", "test", "exam")
    assert_equal(result, "exam examing examed", "string.replace test->exam")
    
    // long strings with many replacements
    result = string.replace("aaaa bbbb aaaa cccc aaaa", "aaaa", "AAAA")
    assert_equal(result, "AAAA bbbb AAAA cccc AAAA", "string.replace many aaaa->AAAA")
    
    result = string.replace("aaaaaaaaaa", "aa", "a")
    assert_equal(result, "aaaaa", "string.replace aa->a in aaaaaaaaaa")
    
    // replace with same string
    result = string.replace("same same same", "same", "same")
    assert_equal(result, "same same same", "string.replace same->same")
    
    result = string.replace("no change", "xyz", "abc")
    assert_equal(result, "no change", "string.replace no matches")
    
    // whitespace patterns
    result = string.replace("  spaces  everywhere  ", "  ", " ")
    assert_equal(result, " spaces everywhere ", "string.replace double spaces->single")
    
    result = string.replace("one  two   three", "  ", " ")
    assert_equal(result, "one two  three", "string.replace multiple spaces")
    
    // mixed case patterns
    result = string.replace("AaAa", "A", "B")
    assert_equal(result, "BaBa", "string.replace uppercase A->B")
    
    result = string.replace("aAaA", "a", "b")
    assert_equal(result, "bAbA", "string.replace lowercase a->b")
    
    // punctuation and symbols
    result = string.replace("!!!", "!", "?")
    assert_equal(result, "???", "string.replace !->?")
    
    result = string.replace("### ### ###", "#", "@")
    assert_equal(result, "@@@ @@@ @@@", "string.replace #->@")
    
    result = string.replace("a$$b$$c", "$$", "$")
    assert_equal(result, "a$b$c", "string.replace $$->$")

// test: all string functions return none for invalid input types
function string_all_none_checks()
    num = 123
    bool_val = true
    nil_val = none
    
    function test_len(val)
        result = string.length(val)
        return result
    
    assert_equal(test_len(num), none, "len(number)")
    assert_equal(test_len(bool_val), none, "len(bool)")
    assert_equal(test_len(nil_val), none, "len(none)")
    
    function test_upper(val)
        return string.upper(val)
    
    assert_equal(test_upper(num), none, "upper(number)")
    assert_equal(test_upper(bool_val), none, "upper(bool)")
    assert_equal(test_upper(nil_val), none, "upper(none)")
    
    function test_lower(val)
        return string.lower(val)
    
    assert_equal(test_lower(num), none, "lower(number)")
    assert_equal(test_lower(bool_val), none, "lower(bool)")
    assert_equal(test_lower(nil_val), none, "lower(none)")
    
    function test_trim(val)
        return string.trim(val)
    
    assert_equal(test_trim(num), none, "trim(number)")
    assert_equal(test_trim(bool_val), none, "trim(bool)")
    assert_equal(test_trim(nil_val), none, "trim(none)")
    
    function test_find(a, b)
        return string.find(a, b)
    
    assert_equal(test_find(num, "test"), none, "find(num,str)")
    assert_equal(test_find("test", num), none, "find(str,num)")
    assert_equal(test_find(bool_val, "test"), none, "find(bool,str)")
    assert_equal(test_find("test", bool_val), none, "find(str,bool)")
    assert_equal(test_find(nil_val, "test"), none, "find(none,str)")
    assert_equal(test_find("test", nil_val), none, "find(str,none)")
    
    function test_replace(a, b, c)
        return string.replace(a, b, c)
    
    assert_equal(test_replace(num, "a", "b"), none, "replace(num,str,str)")
    assert_equal(test_replace("test", num, "b"), none, "replace(str,num,str)")
    assert_equal(test_replace("test", "a", num), none, "replace(str,str,num)")
    assert_equal(test_replace(nil_val, "a", "b"), none, "replace(none,str,str)")
    assert_equal(test_replace("test", nil_val, "b"), none, "replace(str,none,str)")
    assert_equal(test_replace("test", "a", nil_val), none, "replace(str,str,none)")
    
    function test_sub(a, b, c)
        return string.slice(a, b, c)
    
    assert_equal(test_sub(num, 0, 5), none, "sub(num,int,int)")
    assert_equal(test_sub("test", "0", 5), none, "sub(str,str,int)")
    assert_equal(test_sub("test", 0, "5"), none, "sub(str,int,str)")
    assert_equal(test_sub(nil_val, 0, 5), none, "sub(none,int,int)")
    assert_equal(test_sub("test", nil_val, 5), none, "sub(str,none,int)")
    assert_equal(test_sub("test", 0, nil_val), none, "sub(str,int,none)")
    
    function test_split(val)
        return string.split(val, ",")
    
    assert_equal(test_split(num), none, "split(number)")
    assert_equal(test_split(bool_val), none, "split(bool)")
    assert_equal(test_split(nil_val), none, "split(none)")
    
    function test_join(val, sep)
        return string.join(val, sep)
    
    assert_equal(test_join(num, ","), none, "join(number,str)")
    assert_equal(test_join(bool_val, ","), none, "join(bool,str)")
    assert_equal(test_join(nil_val, ","), none, "join(none,str)")
    assert_equal(test_join("not table", ","), none, "join(string,str)")

// execute all string library test cases
string_length()
string_lower()
string_upper()
string_slice()
string_split()
string_join()
string_trim()
string_find()
string_replace()
string_all_none_checks()

// report completion of string library tests
os.output("[PASS] libraries/string.apex")