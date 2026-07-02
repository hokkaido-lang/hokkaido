use crate::cubical::syntax::{Term, nat_to_int, show_term};

// Minimal JSON value type (no external dependencies)
pub enum JsVal {
    Null,
    Bool(bool),
    Num(i64),
    Str(String),
    Arr(Vec<JsVal>),
    Obj(Vec<(String, JsVal)>),
}

impl JsVal {
    fn esc(s: &str) -> String {
        let mut out = String::with_capacity(s.len() + 2);
        out.push('"');
        for c in s.chars() {
            match c {
                '\\' => out.push_str("\\\\"),
                '"' => out.push_str("\\\""),
                '\n' => out.push_str("\\n"),
                '\r' => out.push_str("\\r"),
                '\t' => out.push_str("\\t"),
                c => out.push(c),
            }
        }
        out.push('"');
        out
    }

    pub fn to_string(&self) -> String {
        match self {
            JsVal::Null => "null".into(),
            JsVal::Bool(b) => (if *b { "true" } else { "false" }).into(),
            JsVal::Num(n) => n.to_string(),
            JsVal::Str(s) => Self::esc(s),
            JsVal::Arr(a) => {
                let items: Vec<String> = a.iter().map(|v| v.to_string()).collect();
                format!("[{}]", items.join(","))
            }
            JsVal::Obj(pairs) => {
                let items: Vec<String> = pairs
                    .iter()
                    .map(|(k, v)| format!("{}:{}", Self::esc(k), v.to_string()))
                    .collect();
                format!("{{{}}}", items.join(","))
            }
        }
    }
}

fn kv(k: &str, v: JsVal) -> (String, JsVal) {
    (k.to_string(), v)
}

fn nat_json(t: &Term) -> JsVal {
    if let Some(n) = nat_to_int(t) {
        JsVal::Obj(vec![
            kv("kind", JsVal::Str("nat".into())),
            kv("value", JsVal::Num(n)),
        ])
    } else {
        term_to_json(t)
    }
}

fn bool_json(val: bool) -> JsVal {
    JsVal::Obj(vec![
        kv("kind", JsVal::Str("bool".into())),
        kv("value", JsVal::Bool(val)),
    ])
}

fn constructor_json(data: &str, con: &str, args: &[Term]) -> JsVal {
    let elem_json: Vec<JsVal> = args.iter().map(term_to_json).collect();
    JsVal::Obj(vec![
        kv("kind", JsVal::Str("constructor".into())),
        kv("data", JsVal::Str(data.into())),
        kv("constructor", JsVal::Str(con.into())),
        kv("args", JsVal::Arr(elem_json)),
    ])
}

fn detect_cons_chain(t: &Term) -> Option<Vec<&Term>> {
    match t {
        Term::TCon(_, c, args) if c == "nil" && args.is_empty() => Some(vec![]),
        Term::TCon(_, c, args) if c == "cons" && args.len() >= 2 => {
            let head = &args[0];
            let tail = &args[1];
            detect_cons_chain(tail).map(|mut rest| {
                rest.insert(0, head);
                rest
            })
        }
        _ => None,
    }
}

pub fn term_to_json(t: &Term) -> JsVal {
    match t {
        Term::TCon(d, c, args) if d == "Nat" => match (c.as_str(), args.as_slice()) {
            ("zero", []) => nat_json(t),
            ("suc", [_]) => nat_json(t),
            _ => constructor_json(d, c, args),
        },
        Term::TCon(d, c, args)
            if d == "Bool" && c == "true" && args.is_empty() =>
        {
            bool_json(true)
        }
        Term::TCon(d, c, args)
            if d == "Bool" && c == "false" && args.is_empty() =>
        {
            bool_json(false)
        }
        Term::TPair(a, b) => JsVal::Obj(vec![
            kv("kind", JsVal::Str("pair".into())),
            kv("first", term_to_json(a)),
            kv("second", term_to_json(b)),
        ]),
        Term::TCon(d, c, args) if detect_cons_chain(t).is_some() => {
            let elems = detect_cons_chain(t).unwrap();
            JsVal::Obj(vec![
                kv("kind", JsVal::Str("array".into())),
                kv(
                    "elements",
                    JsVal::Arr(elems.iter().map(|e| term_to_json(e)).collect()),
                ),
            ])
        }
        Term::TCon(d, c, args) => constructor_json(d, c, args),
        _ => JsVal::Obj(vec![
            kv("kind", JsVal::Str("string".into())),
            kv("value", JsVal::Str(show_term(&[], t))),
        ]),
    }
}
