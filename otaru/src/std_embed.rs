/// Embedded std library files for the Hokkaido compiler.
/// These are compiled into the binary so otaru is self-contained
/// and does not depend on finding std/ at runtime.

pub struct StdFile {
    pub path: &'static str,
    pub content: &'static str,
}

pub fn files() -> Vec<StdFile> {
    vec![
        StdFile {
            path: "hk.mod",
            content: "",
        },
        StdFile {
            path: "hof.hk",
            content: r#"package std

pub fn twice(f: fn(int) -> int, x: int) -> int {
    return f(f(x))
}

pub fn thrice(f: fn(int) -> int, x: int) -> int {
    return f(f(f(x)))
}

pub fn compose(f: fn(int) -> int, g: fn(int) -> int, x: int) -> int {
    return f(g(x))
}

pub fn apply_n(f: fn(int) -> int, n: int, x: int) -> int {
    if n <= 0 {
        return x
    }
    return apply_n(f, n - 1, f(x))
}

pub fn fold_int(f: fn(int, int) -> int, init: int, arr: int[], len: int) -> int {
    if len <= 0 {
        return init
    }
    let i: int = len - 1
    return f(arr[i], fold_int(f, init, arr, i))
}

pub fn any_int(f: fn(int) -> bool, arr: int[], len: int) -> bool {
    if len <= 0 {
        return false
    }
    let i: int = len - 1
    if f(arr[i]) {
        return true
    }
    return any_int(f, arr, i)
}

pub fn all_int(f: fn(int) -> bool, arr: int[], len: int) -> bool {
    if len <= 0 {
        return true
    }
    let i: int = len - 1
    let val: int = arr[i]
    if f(val) {
        return all_int(f, arr, i)
    }
    return false
}

pub fn map_int_into(f: fn(int) -> int, src: int[], dst: int[], len: int) -> int {
    if len <= 0 {
        return 0
    }
    let i: int = len - 1
    dst[i] = f(src[i])
    return map_int_into(f, src, dst, i)
}
"#,
        },
    ]
}
