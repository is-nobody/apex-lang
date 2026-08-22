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
    len = string.length("Hello")
    assert_equal(len, 5, "string.length Hello")
    
    len = string.length("")
    assert_equal(len, 0, "string.length empty")

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
    result = string.slice("Hello World", 1, 5)
    assert_equal(result, "Hello", "string.slice english 1-5")
    
    result = string.slice("Hello World", 7, 11)
    assert_equal(result, "World", "string.slice english 7-11")
    
    result = string.slice("你好世界", 1, 2)
    assert_equal(result, "你好", "string.slice chinese 1-2")
    
    result = string.slice("你好世界", 3, 4)
    assert_equal(result, "世界", "string.slice chinese 3-4")
    
    result = string.slice("Привет Мир", 1, 6)
    assert_equal(result, "Привет", "string.slice russian 1-6")
    
    result = string.slice("Привет Мир", 8, 10)
    assert_equal(result, "Мир", "string.slice russian 8-10")
    
    result = string.slice("السلام عليكم", 1, 6)
    assert_equal(result, "السلام", "string.slice arabic 1-6")
    
    result = string.slice("السلام عليكم", 8, 13)
    assert_equal(result, "عليكم", "string.slice arabic 8-13")
    
    result = string.slice("Hello Привет 你好", 1, 5)
    assert_equal(result, "Hello", "string.slice mixed 1-5")
    
    result = string.slice("Hello Привет 你好", 7, 12)
    assert_equal(result, "Привет", "string.slice mixed 7-12")
    
    result = string.slice("Hello Привет 你好", 14, 15)
    assert_equal(result, "你好", "string.slice mixed 14-15")
    
    result = string.slice("", 1, 5)
    assert_equal(result, "", "string.slice empty")
    
    result = string.slice("Hello", 1, 10)
    assert_equal(result, "Hello", "string.slice beyond end")
    
    result = string.slice("Hello", -4, 5)
    assert_equal(result, "Hello", "string.slice negative start")
    
    result = string.slice("😊🌟", 1, 1)
    assert_equal(result, "😊", "string.slice emoji 1-1")
    
    result = string.slice("😊🌟", 2, 2)
    assert_equal(result, "🌟", "string.slice emoji 2-2")

// test: split string by delimiter into table
function string_split()
    parts = string.split("a,b,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split[1]")
        assert_equal(parts[2], "b", "string.split[2]")
        assert_equal(parts[3], "c", "string.split[3]")

// test: join table elements into string with separator
function string_join()
    parts = string.split("a,b,c", ",")
    result = string.join(parts, "-")
    assert_equal(result, "a-b-c", "string.join with dash")
    
    result = string.join(parts, "")
    assert_equal(result, "abc", "string.join empty separator")

// test: trim whitespace from both ends
function string_trim()
    result = string.trim("  hello  ")
    assert_equal(result, "hello", "string.trim spaces")
    
    result = string.trim("\t\nhello\t\n")
    assert_equal(result, "hello", "string.trim tabs newlines")
    
    result = string.trim("hello")
    assert_equal(result, "hello", "string.trim no change")

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