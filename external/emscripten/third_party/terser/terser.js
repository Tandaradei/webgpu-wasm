/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

function characters(str) {
    return str.split("");
}

function member(name, array) {
    return array.indexOf(name) >= 0;
}

function find_if(func, array) {
    for (var i = 0, n = array.length; i < n; ++i) {
        if (func(array[i]))
            return array[i];
    }
}

function repeat_string(str, i) {
    if (i <= 0) return "";
    if (i == 1) return str;
    var d = repeat_string(str, i >> 1);
    d += d;
    if (i & 1) d += str;
    return d;
}

function configure_error_stack(fn) {
    Object.defineProperty(fn.prototype, "stack", {
        get: function() {
            var err = new Error(this.message);
            err.name = this.name;
            try {
                throw err;
            } catch(e) {
                return e.stack;
            }
        }
    });
}

function DefaultsError(msg, defs) {
    this.message = msg;
    this.defs = defs;
}
DefaultsError.prototype = Object.create(Error.prototype);
DefaultsError.prototype.constructor = DefaultsError;
DefaultsError.prototype.name = "DefaultsError";
configure_error_stack(DefaultsError);

DefaultsError.croak = function(msg, defs) {
    throw new DefaultsError(msg, defs);
};

function defaults(args, defs, croak) {
    if (args === true)
        args = {};
    var ret = args || {};
    if (croak) for (var i in ret) if (HOP(ret, i) && !HOP(defs, i))
        DefaultsError.croak("`" + i + "` is not a supported option", defs);
    for (var i in defs) if (HOP(defs, i)) {
        ret[i] = (args && HOP(args, i)) ? args[i] : defs[i];
    }
    return ret;
}

function merge(obj, ext) {
    var count = 0;
    for (var i in ext) if (HOP(ext, i)) {
        obj[i] = ext[i];
        count++;
    }
    return count;
}

function noop() {}
function return_false() { return false; }
function return_true() { return true; }
function return_this() { return this; }
function return_null() { return null; }

var MAP = (function() {
    function MAP(a, f, backwards) {
        var ret = [], top = [], i;
        function doit() {
            var val = f(a[i], i);
            var is_last = val instanceof Last;
            if (is_last) val = val.v;
            if (val instanceof AtTop) {
                val = val.v;
                if (val instanceof Splice) {
                    top.push.apply(top, backwards ? val.v.slice().reverse() : val.v);
                } else {
                    top.push(val);
                }
            } else if (val !== skip) {
                if (val instanceof Splice) {
                    ret.push.apply(ret, backwards ? val.v.slice().reverse() : val.v);
                } else {
                    ret.push(val);
                }
            }
            return is_last;
        }
        if (a instanceof Array) {
            if (backwards) {
                for (i = a.length; --i >= 0;) if (doit()) break;
                ret.reverse();
                top.reverse();
            } else {
                for (i = 0; i < a.length; ++i) if (doit()) break;
            }
        } else {
            for (i in a) if (HOP(a, i)) if (doit()) break;
        }
        return top.concat(ret);
    }
    MAP.at_top = function(val) { return new AtTop(val); };
    MAP.splice = function(val) { return new Splice(val); };
    MAP.last = function(val) { return new Last(val); };
    var skip = MAP.skip = {};
    function AtTop(val) { this.v = val; }
    function Splice(val) { this.v = val; }
    function Last(val) { this.v = val; }
    return MAP;
})();

function push_uniq(array, el) {
    if (array.indexOf(el) < 0)
        array.push(el);
}

function string_template(text, props) {
    return text.replace(/\{(.+?)\}/g, function(str, p) {
        return props && props[p];
    });
}

function remove(array, el) {
    for (var i = array.length; --i >= 0;) {
        if (array[i] === el) array.splice(i, 1);
    }
}

function mergeSort(array, cmp) {
    if (array.length < 2) return array.slice();
    function merge(a, b) {
        var r = [], ai = 0, bi = 0, i = 0;
        while (ai < a.length && bi < b.length) {
            cmp(a[ai], b[bi]) <= 0
                ? r[i++] = a[ai++]
                : r[i++] = b[bi++];
        }
        if (ai < a.length) r.push.apply(r, a.slice(ai));
        if (bi < b.length) r.push.apply(r, b.slice(bi));
        return r;
    }
    function _ms(a) {
        if (a.length <= 1)
            return a;
        var m = Math.floor(a.length / 2), left = a.slice(0, m), right = a.slice(m);
        left = _ms(left);
        right = _ms(right);
        return merge(left, right);
    }
    return _ms(array);
}

// this function is taken from Acorn [1], written by Marijn Haverbeke
// [1] https://github.com/marijnh/acorn
function makePredicate(words) {
    if (!(words instanceof Array)) words = words.split(" ");
    var f = "", cats = [];
    out: for (var i = 0; i < words.length; ++i) {
        for (var j = 0; j < cats.length; ++j)
            if (cats[j][0].length == words[i].length) {
                cats[j].push(words[i]);
                continue out;
            }
        cats.push([words[i]]);
    }
    function quote(word) {
        return JSON.stringify(word).replace(/[\u2028\u2029]/g, function(s) {
            switch (s) {
                case "\u2028": return "\\u2028";
                case "\u2029": return "\\u2029";
            }
            return s;
        });
    }
    function compareTo(arr) {
        if (arr.length == 1) return f += "return str === " + quote(arr[0]) + ";";
        f += "switch(str){";
        for (var i = 0; i < arr.length; ++i) f += "case " + quote(arr[i]) + ":";
        f += "return true}return false;";
    }
    // When there are more than three length categories, an outer
    // switch first dispatches on the lengths, to save on comparisons.
    if (cats.length > 3) {
        cats.sort(function(a, b) {return b.length - a.length;});
        f += "switch(str.length){";
        for (var i = 0; i < cats.length; ++i) {
            var cat = cats[i];
            f += "case " + cat[0].length + ":";
            compareTo(cat);
        }
        f += "}";
        // Otherwise, simply generate a flat `switch` statement.
    } else {
        compareTo(words);
    }
    return new Function("str", f);
}

function all(array, predicate) {
    for (var i = array.length; --i >= 0;)
        if (!predicate(array[i]))
            return false;
    return true;
}

function Dictionary() {
    this._values = Object.create(null);
    this._size = 0;
}
Dictionary.prototype = {
    set: function(key, val) {
        if (!this.has(key)) ++this._size;
        this._values["$" + key] = val;
        return this;
    },
    add: function(key, val) {
        if (this.has(key)) {
            this.get(key).push(val);
        } else {
            this.set(key, [ val ]);
        }
        return this;
    },
    get: function(key) { return this._values["$" + key]; },
    del: function(key) {
        if (this.has(key)) {
            --this._size;
            delete this._values["$" + key];
        }
        return this;
    },
    has: function(key) { return ("$" + key) in this._values; },
    each: function(f) {
        for (var i in this._values)
            f(this._values[i], i.substr(1));
    },
    size: function() {
        return this._size;
    },
    map: function(f) {
        var ret = [];
        for (var i in this._values)
            ret.push(f(this._values[i], i.substr(1)));
        return ret;
    },
    clone: function() {
        var ret = new Dictionary();
        for (var i in this._values)
            ret._values[i] = this._values[i];
        ret._size = this._size;
        return ret;
    },
    toObject: function() { return this._values; }
};
Dictionary.fromObject = function(obj) {
    var dict = new Dictionary();
    dict._size = merge(dict._values, obj);
    return dict;
};
exports.Dictionary = Dictionary;

function HOP(obj, prop) {
    return Object.prototype.hasOwnProperty.call(obj, prop);
}

// return true if the node at the top of the stack (that means the
// innermost node in the current output) is lexically the first in
// a statement.
function first_in_statement(stack) {
    var node = stack.parent(-1);
    for (var i = 0, p; p = stack.parent(i); i++) {
        if (p instanceof AST_Statement && p.body === node)
            return true;
        if ((p instanceof AST_Sequence      && p.expressions[0] === node) ||
            (p.TYPE == "Call"               && p.expression === node ) ||
            (p instanceof AST_Dot           && p.expression === node ) ||
            (p instanceof AST_Sub           && p.expression === node ) ||
            (p instanceof AST_Conditional   && p.condition === node  ) ||
            (p instanceof AST_Binary        && p.left === node       ) ||
            (p instanceof AST_UnaryPostfix  && p.expression === node )
        ) {
            node = p;
        } else {
            return false;
        }
    }
}

function keep_name(keep_setting, name) {
    return keep_setting === true
        || (keep_setting instanceof RegExp && keep_setting.test(name));
}
/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

function DEFNODE(type, props, methods, base) {
    if (arguments.length < 4) base = AST_Node;
    if (!props) props = [];
    else props = props.split(/\s+/);
    var self_props = props;
    if (base && base.PROPS)
        props = props.concat(base.PROPS);
    var code = "return function AST_" + type + "(props){ if (props) { ";
    for (var i = props.length; --i >= 0;) {
        code += "this." + props[i] + " = props." + props[i] + ";";
    }
    var proto = base && new base;
    if (proto && proto.initialize || (methods && methods.initialize))
        code += "this.initialize();";
    code += "}}";
    var ctor = new Function(code)();
    if (proto) {
        ctor.prototype = proto;
        ctor.BASE = base;
    }
    if (base) base.SUBCLASSES.push(ctor);
    ctor.prototype.CTOR = ctor;
    ctor.PROPS = props || null;
    ctor.SELF_PROPS = self_props;
    ctor.SUBCLASSES = [];
    if (type) {
        ctor.prototype.TYPE = ctor.TYPE = type;
    }
    if (methods) for (i in methods) if (HOP(methods, i)) {
        if (/^\$/.test(i)) {
            ctor[i.substr(1)] = methods[i];
        } else {
            ctor.prototype[i] = methods[i];
        }
    }
    ctor.DEFMETHOD = function(name, method) {
        this.prototype[name] = method;
    };
    if (typeof exports !== "undefined") {
        exports["AST_" + type] = ctor;
    }
    return ctor;
}

var AST_Token = DEFNODE("Token", "type value line col pos endline endcol endpos nlb comments_before comments_after file raw", {
}, null);

var AST_Node = DEFNODE("Node", "start end", {
    _clone: function(deep) {
        if (deep) {
            var self = this.clone();
            return self.transform(new TreeTransformer(function(node) {
                if (node !== self) {
                    return node.clone(true);
                }
            }));
        }
        return new this.CTOR(this);
    },
    clone: function(deep) {
        return this._clone(deep);
    },
    $documentation: "Base class of all AST nodes",
    $propdoc: {
        start: "[AST_Token] The first token of this node",
        end: "[AST_Token] The last token of this node"
    },
    _walk: function(visitor) {
        return visitor._visit(this);
    },
    walk: function(visitor) {
        return this._walk(visitor); // not sure the indirection will be any help
    }
}, null);

AST_Node.warn_function = null;
AST_Node.warn = function(txt, props) {
    if (AST_Node.warn_function)
        AST_Node.warn_function(string_template(txt, props));
};

/* -----[ statements ]----- */

var AST_Statement = DEFNODE("Statement", null, {
    $documentation: "Base class of all statements",
});

var AST_Debugger = DEFNODE("Debugger", null, {
    $documentation: "Represents a debugger statement",
}, AST_Statement);

var AST_Directive = DEFNODE("Directive", "value quote", {
    $documentation: "Represents a directive, like \"use strict\";",
    $propdoc: {
        value: "[string] The value of this directive as a plain string (it's not an AST_String!)",
        quote: "[string] the original quote character"
    },
}, AST_Statement);

var AST_SimpleStatement = DEFNODE("SimpleStatement", "body", {
    $documentation: "A statement consisting of an expression, i.e. a = 1 + 2",
    $propdoc: {
        body: "[AST_Node] an expression node (should not be instanceof AST_Statement)"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.body._walk(visitor);
        });
    }
}, AST_Statement);

// XXX Emscripten localmod: Add a node type for a parenthesized expression so that we can retain
// Closure annotations that need a form "/**annotation*/(expression)"
var AST_ParenthesizedExpression = DEFNODE("ParenthesizedExpression", "body", {
    $documentation: "An explicitly parenthesized expression, i.e. a = (1 + 2)",
    $propdoc: {
        body: "[AST_Node] an expression node (should not be instanceof AST_Statement)"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.body._walk(visitor);
        });
    }
}, AST_Statement);
// XXX End of Emscripten localmod

function walk_body(node, visitor) {
    var body = node.body;
    if (body instanceof AST_Node) {
        body._walk(visitor);
    } else for (var i = 0, len = body.length; i < len; i++) {
        body[i]._walk(visitor);
    }
}

function clone_block_scope(deep) {
    var clone = this._clone(deep);
    if (this.block_scope) {
        // TODO this is sometimes undefined during compression.
        // But it should always have a value!
        clone.block_scope = this.block_scope.clone();
    }
    return clone;
}

var AST_Block = DEFNODE("Block", "body block_scope", {
    $documentation: "A body of statements (usually braced)",
    $propdoc: {
        body: "[AST_Statement*] an array of statements",
        block_scope: "[AST_Scope] the block scope"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            walk_body(this, visitor);
        });
    },
    clone: clone_block_scope
}, AST_Statement);

var AST_BlockStatement = DEFNODE("BlockStatement", null, {
    $documentation: "A block statement",
}, AST_Block);

var AST_EmptyStatement = DEFNODE("EmptyStatement", null, {
    $documentation: "The empty statement (empty block or simply a semicolon)"
}, AST_Statement);

var AST_StatementWithBody = DEFNODE("StatementWithBody", "body", {
    $documentation: "Base class for all statements that contain one nested body: `For`, `ForIn`, `Do`, `While`, `With`",
    $propdoc: {
        body: "[AST_Statement] the body; this should always be present, even if it's an AST_EmptyStatement"
    }
}, AST_Statement);

var AST_LabeledStatement = DEFNODE("LabeledStatement", "label", {
    $documentation: "Statement with a label",
    $propdoc: {
        label: "[AST_Label] a label definition"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.label._walk(visitor);
            this.body._walk(visitor);
        });
    },
    clone: function(deep) {
        var node = this._clone(deep);
        if (deep) {
            var label = node.label;
            var def = this.label;
            node.walk(new TreeWalker(function(node) {
                if (node instanceof AST_LoopControl
                    && node.label && node.label.thedef === def) {
                    node.label.thedef = label;
                    label.references.push(node);
                }
            }));
        }
        return node;
    }
}, AST_StatementWithBody);

var AST_IterationStatement = DEFNODE("IterationStatement", "block_scope", {
    $documentation: "Internal class.  All loops inherit from it.",
    $propdoc: {
        block_scope: "[AST_Scope] the block scope for this iteration statement."
    },
    clone: clone_block_scope
}, AST_StatementWithBody);

var AST_DWLoop = DEFNODE("DWLoop", "condition", {
    $documentation: "Base class for do/while statements",
    $propdoc: {
        condition: "[AST_Node] the loop condition.  Should not be instanceof AST_Statement"
    }
}, AST_IterationStatement);

var AST_Do = DEFNODE("Do", null, {
    $documentation: "A `do` statement",
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.body._walk(visitor);
            this.condition._walk(visitor);
        });
    }
}, AST_DWLoop);

var AST_While = DEFNODE("While", null, {
    $documentation: "A `while` statement",
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.condition._walk(visitor);
            this.body._walk(visitor);
        });
    }
}, AST_DWLoop);

var AST_For = DEFNODE("For", "init condition step", {
    $documentation: "A `for` statement",
    $propdoc: {
        init: "[AST_Node?] the `for` initialization code, or null if empty",
        condition: "[AST_Node?] the `for` termination clause, or null if empty",
        step: "[AST_Node?] the `for` update clause, or null if empty"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.init) this.init._walk(visitor);
            if (this.condition) this.condition._walk(visitor);
            if (this.step) this.step._walk(visitor);
            this.body._walk(visitor);
        });
    }
}, AST_IterationStatement);

var AST_ForIn = DEFNODE("ForIn", "init object", {
    $documentation: "A `for ... in` statement",
    $propdoc: {
        init: "[AST_Node] the `for/in` initialization code",
        object: "[AST_Node] the object that we're looping through"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.init._walk(visitor);
            this.object._walk(visitor);
            this.body._walk(visitor);
        });
    }
}, AST_IterationStatement);

var AST_ForOf = DEFNODE("ForOf", "await", {
    $documentation: "A `for ... of` statement",
}, AST_ForIn);

var AST_With = DEFNODE("With", "expression", {
    $documentation: "A `with` statement",
    $propdoc: {
        expression: "[AST_Node] the `with` expression"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
            this.body._walk(visitor);
        });
    }
}, AST_StatementWithBody);

/* -----[ scope and functions ]----- */

var AST_Scope = DEFNODE("Scope", "variables functions uses_with uses_eval parent_scope enclosed cname", {
    $documentation: "Base class for all statements introducing a lexical scope",
    $propdoc: {
        variables: "[Object/S] a map of name -> SymbolDef for all variables/functions defined in this scope",
        functions: "[Object/S] like `variables`, but only lists function declarations",
        uses_with: "[boolean/S] tells whether this scope uses the `with` statement",
        uses_eval: "[boolean/S] tells whether this scope contains a direct call to the global `eval`",
        parent_scope: "[AST_Scope?/S] link to the parent scope",
        enclosed: "[SymbolDef*/S] a list of all symbol definitions that are accessed from this scope or any subscopes",
        cname: "[integer/S] current index for mangling variables (used internally by the mangler)",
    },
    get_defun_scope: function() {
        var self = this;
        while (self.is_block_scope()) {
            self = self.parent_scope;
        }
        return self;
    },
    clone: function(deep) {
        var node = this._clone(deep);
        if (this.variables) node.variables = this.variables.clone();
        if (this.functions) node.functions = this.functions.clone();
        if (this.enclosed) node.enclosed = this.enclosed.slice();
        return node;
    },
    pinned: function() {
        return this.uses_eval || this.uses_with;
    }
}, AST_Block);

var AST_Toplevel = DEFNODE("Toplevel", "globals", {
    $documentation: "The toplevel scope",
    $propdoc: {
        globals: "[Object/S] a map of name -> SymbolDef for all undeclared names",
    },
    wrap_commonjs: function(name) {
        var body = this.body;
        var wrapped_tl = "(function(exports){'$ORIG';})(typeof " + name + "=='undefined'?(" + name + "={}):" + name + ");";
        wrapped_tl = parse(wrapped_tl);
        wrapped_tl = wrapped_tl.transform(new TreeTransformer(function(node) {
            if (node instanceof AST_Directive && node.value == "$ORIG") {
                return MAP.splice(body);
            }
        }));
        return wrapped_tl;
    },
    wrap_enclose: function(args_values) {
        if (typeof args_values != "string") args_values = "";
        var index = args_values.indexOf(":");
        if (index < 0) index = args_values.length;
        var body = this.body;
        return parse([
            "(function(",
            args_values.slice(0, index),
            '){"$ORIG"})(',
            args_values.slice(index + 1),
            ")"
        ].join("")).transform(new TreeTransformer(function(node) {
            if (node instanceof AST_Directive && node.value == "$ORIG") {
                return MAP.splice(body);
            }
        }));
    }
}, AST_Scope);

var AST_Expansion = DEFNODE("Expansion", "expression", {
    $documentation: "An expandible argument, such as ...rest, a splat, such as [1,2,...all], or an expansion in a variable declaration, such as var [first, ...rest] = list",
    $propdoc: {
        expression: "[AST_Node] the thing to be expanded"
    },
    _walk: function(visitor) {
        var self = this;
        return visitor._visit(this, function() {
            self.expression.walk(visitor);
        });
    }
});

var AST_Lambda = DEFNODE("Lambda", "name argnames uses_arguments is_generator async", {
    $documentation: "Base class for functions",
    $propdoc: {
        name: "[AST_SymbolDeclaration?] the name of this function",
        argnames: "[AST_SymbolFunarg|AST_Destructuring|AST_Expansion|AST_DefaultAssign*] array of function arguments, destructurings, or expanding arguments",
        uses_arguments: "[boolean/S] tells whether this function accesses the arguments array",
        is_generator: "[boolean] is this a generator method",
        async: "[boolean] is this method async",
    },
    args_as_names: function () {
        var out = [];
        for (var i = 0; i < this.argnames.length; i++) {
            if (this.argnames[i] instanceof AST_Destructuring) {
                out = out.concat(this.argnames[i].all_symbols());
            } else {
                out.push(this.argnames[i]);
            }
        }
        return out;
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.name) this.name._walk(visitor);
            var argnames = this.argnames;
            for (var i = 0, len = argnames.length; i < len; i++) {
                argnames[i]._walk(visitor);
            }
            walk_body(this, visitor);
        });
    }
}, AST_Scope);

var AST_Accessor = DEFNODE("Accessor", null, {
    $documentation: "A setter/getter function.  The `name` property is always null."
}, AST_Lambda);

var AST_Function = DEFNODE("Function", "inlined", {
    $documentation: "A function expression"
}, AST_Lambda);

var AST_Arrow = DEFNODE("Arrow", "inlined", {
    $documentation: "An ES6 Arrow function ((a) => b)"
}, AST_Lambda);

var AST_Defun = DEFNODE("Defun", "inlined", {
    $documentation: "A function definition"
}, AST_Lambda);

/* -----[ DESTRUCTURING ]----- */
var AST_Destructuring = DEFNODE("Destructuring", "names is_array", {
    $documentation: "A destructuring of several names. Used in destructuring assignment and with destructuring function argument names",
    $propdoc: {
        "names": "[AST_Node*] Array of properties or elements",
        "is_array": "[Boolean] Whether the destructuring represents an object or array"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.names.forEach(function(name) {
                name._walk(visitor);
            });
        });
    },
    all_symbols: function() {
        var out = [];
        this.walk(new TreeWalker(function (node) {
            if (node instanceof AST_Symbol) {
                out.push(node);
            }
            if (node instanceof AST_Expansion) {
                out.push(node.expression);
            }
        }));
        return out;
    }
});

var AST_PrefixedTemplateString = DEFNODE("PrefixedTemplateString", "template_string prefix", {
    $documentation: "A templatestring with a prefix, such as String.raw`foobarbaz`",
    $propdoc: {
        template_string: "[AST_TemplateString] The template string",
        prefix: "[AST_SymbolRef|AST_PropAccess] The prefix, which can be a symbol such as `foo` or a dotted expression such as `String.raw`."
    },
    _walk: function(visitor) {
        this.prefix._walk(visitor);
        this.template_string._walk(visitor);
    }
});

var AST_TemplateString = DEFNODE("TemplateString", "segments", {
    $documentation: "A template string literal",
    $propdoc: {
        segments: "[AST_Node*] One or more segments, starting with AST_TemplateSegment. AST_Node may follow AST_TemplateSegment, but each AST_Node must be followed by AST_TemplateSegment."
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.segments.forEach(function(seg) {
                seg._walk(visitor);
            });
        });
    }
});

var AST_TemplateSegment = DEFNODE("TemplateSegment", "value raw", {
    $documentation: "A segment of a template string literal",
    $propdoc: {
        value: "Content of the segment",
        raw: "Raw content of the segment"
    }
});

/* -----[ JUMPS ]----- */

var AST_Jump = DEFNODE("Jump", null, {
    $documentation: "Base class for “jumps” (for now that's `return`, `throw`, `break` and `continue`)"
}, AST_Statement);

var AST_Exit = DEFNODE("Exit", "value", {
    $documentation: "Base class for “exits” (`return` and `throw`)",
    $propdoc: {
        value: "[AST_Node?] the value returned or thrown by this statement; could be null for AST_Return"
    },
    _walk: function(visitor) {
        return visitor._visit(this, this.value && function() {
            this.value._walk(visitor);
        });
    }
}, AST_Jump);

var AST_Return = DEFNODE("Return", null, {
    $documentation: "A `return` statement"
}, AST_Exit);

var AST_Throw = DEFNODE("Throw", null, {
    $documentation: "A `throw` statement"
}, AST_Exit);

var AST_LoopControl = DEFNODE("LoopControl", "label", {
    $documentation: "Base class for loop control statements (`break` and `continue`)",
    $propdoc: {
        label: "[AST_LabelRef?] the label, or null if none",
    },
    _walk: function(visitor) {
        return visitor._visit(this, this.label && function() {
            this.label._walk(visitor);
        });
    }
}, AST_Jump);

var AST_Break = DEFNODE("Break", null, {
    $documentation: "A `break` statement"
}, AST_LoopControl);

var AST_Continue = DEFNODE("Continue", null, {
    $documentation: "A `continue` statement"
}, AST_LoopControl);

/* -----[ IF ]----- */

var AST_If = DEFNODE("If", "condition alternative", {
    $documentation: "A `if` statement",
    $propdoc: {
        condition: "[AST_Node] the `if` condition",
        alternative: "[AST_Statement?] the `else` part, or null if not present"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.condition._walk(visitor);
            this.body._walk(visitor);
            if (this.alternative) this.alternative._walk(visitor);
        });
    }
}, AST_StatementWithBody);

/* -----[ SWITCH ]----- */

var AST_Switch = DEFNODE("Switch", "expression", {
    $documentation: "A `switch` statement",
    $propdoc: {
        expression: "[AST_Node] the `switch` “discriminant”"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
            walk_body(this, visitor);
        });
    }
}, AST_Block);

var AST_SwitchBranch = DEFNODE("SwitchBranch", null, {
    $documentation: "Base class for `switch` branches",
}, AST_Block);

var AST_Default = DEFNODE("Default", null, {
    $documentation: "A `default` switch branch",
}, AST_SwitchBranch);

var AST_Case = DEFNODE("Case", "expression", {
    $documentation: "A `case` switch branch",
    $propdoc: {
        expression: "[AST_Node] the `case` expression"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
            walk_body(this, visitor);
        });
    }
}, AST_SwitchBranch);

/* -----[ EXCEPTIONS ]----- */

var AST_Try = DEFNODE("Try", "bcatch bfinally", {
    $documentation: "A `try` statement",
    $propdoc: {
        bcatch: "[AST_Catch?] the catch block, or null if not present",
        bfinally: "[AST_Finally?] the finally block, or null if not present"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            walk_body(this, visitor);
            if (this.bcatch) this.bcatch._walk(visitor);
            if (this.bfinally) this.bfinally._walk(visitor);
        });
    }
}, AST_Block);

var AST_Catch = DEFNODE("Catch", "argname", {
    $documentation: "A `catch` node; only makes sense as part of a `try` statement",
    $propdoc: {
        argname: "[AST_SymbolCatch|AST_Destructuring|AST_Expansion|AST_DefaultAssign] symbol for the exception"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.argname) this.argname._walk(visitor);
            walk_body(this, visitor);
        });
    }
}, AST_Block);

var AST_Finally = DEFNODE("Finally", null, {
    $documentation: "A `finally` node; only makes sense as part of a `try` statement"
}, AST_Block);

/* -----[ VAR/CONST ]----- */

var AST_Definitions = DEFNODE("Definitions", "definitions", {
    $documentation: "Base class for `var` or `const` nodes (variable declarations/initializations)",
    $propdoc: {
        definitions: "[AST_VarDef*] array of variable definitions"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            var definitions = this.definitions;
            for (var i = 0, len = definitions.length; i < len; i++) {
                definitions[i]._walk(visitor);
            }
        });
    }
}, AST_Statement);

var AST_Var = DEFNODE("Var", null, {
    $documentation: "A `var` statement"
}, AST_Definitions);

var AST_Let = DEFNODE("Let", null, {
    $documentation: "A `let` statement"
}, AST_Definitions);

var AST_Const = DEFNODE("Const", null, {
    $documentation: "A `const` statement"
}, AST_Definitions);

var AST_NameMapping = DEFNODE("NameMapping", "foreign_name name", {
    $documentation: "The part of the export/import statement that declare names from a module.",
    $propdoc: {
        foreign_name: "[AST_SymbolExportForeign|AST_SymbolImportForeign] The name being exported/imported (as specified in the module)",
        name: "[AST_SymbolExport|AST_SymbolImport] The name as it is visible to this module."
    },
    _walk: function (visitor) {
        return visitor._visit(this, function() {
            this.foreign_name._walk(visitor);
            this.name._walk(visitor);
        });
    }
});

var AST_Import = DEFNODE("Import", "imported_name imported_names module_name", {
    $documentation: "An `import` statement",
    $propdoc: {
        imported_name: "[AST_SymbolImport] The name of the variable holding the module's default export.",
        imported_names: "[AST_NameMapping*] The names of non-default imported variables",
        module_name: "[AST_String] String literal describing where this module came from",
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.imported_name) {
                this.imported_name._walk(visitor);
            }
            if (this.imported_names) {
                this.imported_names.forEach(function(name_import) {
                    name_import._walk(visitor);
                });
            }
            this.module_name._walk(visitor);
        });
    }
});

var AST_Export = DEFNODE("Export", "exported_definition exported_value is_default exported_names module_name", {
    $documentation: "An `export` statement",
    $propdoc: {
        exported_definition: "[AST_Defun|AST_Definitions|AST_DefClass?] An exported definition",
        exported_value: "[AST_Node?] An exported value",
        exported_names: "[AST_NameMapping*?] List of exported names",
        module_name: "[AST_String?] Name of the file to load exports from",
        is_default: "[Boolean] Whether this is the default exported value of this module"
    },
    _walk: function (visitor) {
        visitor._visit(this, function () {
            if (this.exported_definition) {
                this.exported_definition._walk(visitor);
            }
            if (this.exported_value) {
                this.exported_value._walk(visitor);
            }
            if (this.exported_names) {
                this.exported_names.forEach(function(name_export) {
                    name_export._walk(visitor);
                });
            }
            if (this.module_name) {
                this.module_name._walk(visitor);
            }
        });
    }
}, AST_Statement);

var AST_VarDef = DEFNODE("VarDef", "name value", {
    $documentation: "A variable declaration; only appears in a AST_Definitions node",
    $propdoc: {
        name: "[AST_Destructuring|AST_SymbolConst|AST_SymbolLet|AST_SymbolVar] name of the variable",
        value: "[AST_Node?] initializer, or null of there's no initializer"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.name._walk(visitor);
            if (this.value) this.value._walk(visitor);
        });
    }
});

/* -----[ OTHER ]----- */

var AST_Call = DEFNODE("Call", "expression args", {
    $documentation: "A function call expression",
    $propdoc: {
        expression: "[AST_Node] expression to invoke as function",
        args: "[AST_Node*] array of arguments"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            var args = this.args;
            for (var i = 0, len = args.length; i < len; i++) {
                args[i]._walk(visitor);
            }
            this.expression._walk(visitor);
        });
    }
});

var AST_New = DEFNODE("New", null, {
    $documentation: "An object instantiation.  Derives from a function call since it has exactly the same properties"
}, AST_Call);

var AST_Sequence = DEFNODE("Sequence", "expressions", {
    $documentation: "A sequence expression (comma-separated expressions)",
    $propdoc: {
        expressions: "[AST_Node*] array of expressions (at least two)"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expressions.forEach(function(node) {
                node._walk(visitor);
            });
        });
    }
});

var AST_PropAccess = DEFNODE("PropAccess", "expression property", {
    $documentation: "Base class for property access expressions, i.e. `a.foo` or `a[\"foo\"]`",
    $propdoc: {
        expression: "[AST_Node] the “container” expression",
        property: "[AST_Node|string] the property to access.  For AST_Dot this is always a plain string, while for AST_Sub it's an arbitrary AST_Node"
    }
});

var AST_Dot = DEFNODE("Dot", null, {
    $documentation: "A dotted property access expression",
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
        });
    }
}, AST_PropAccess);

var AST_Sub = DEFNODE("Sub", null, {
    $documentation: "Index-style property access, i.e. `a[\"foo\"]`",
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
            this.property._walk(visitor);
        });
    }
}, AST_PropAccess);

var AST_Unary = DEFNODE("Unary", "operator expression", {
    $documentation: "Base class for unary expressions",
    $propdoc: {
        operator: "[string] the operator",
        expression: "[AST_Node] expression that this unary operator applies to"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
        });
    }
});

var AST_UnaryPrefix = DEFNODE("UnaryPrefix", null, {
    $documentation: "Unary prefix expression, i.e. `typeof i` or `++i`"
}, AST_Unary);

var AST_UnaryPostfix = DEFNODE("UnaryPostfix", null, {
    $documentation: "Unary postfix expression, i.e. `i++`"
}, AST_Unary);

var AST_Binary = DEFNODE("Binary", "operator left right", {
    $documentation: "Binary expression, i.e. `a + b`",
    $propdoc: {
        left: "[AST_Node] left-hand side expression",
        operator: "[string] the operator",
        right: "[AST_Node] right-hand side expression"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.left._walk(visitor);
            this.right._walk(visitor);
        });
    }
});

var AST_Conditional = DEFNODE("Conditional", "condition consequent alternative", {
    $documentation: "Conditional expression using the ternary operator, i.e. `a ? b : c`",
    $propdoc: {
        condition: "[AST_Node]",
        consequent: "[AST_Node]",
        alternative: "[AST_Node]"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.condition._walk(visitor);
            this.consequent._walk(visitor);
            this.alternative._walk(visitor);
        });
    }
});

var AST_Assign = DEFNODE("Assign", null, {
    $documentation: "An assignment expression — `a = b + 5`",
}, AST_Binary);

var AST_DefaultAssign = DEFNODE("DefaultAssign", null, {
    $documentation: "A default assignment expression like in `(a = 3) => a`"
}, AST_Binary);

/* -----[ LITERALS ]----- */

var AST_Array = DEFNODE("Array", "elements", {
    $documentation: "An array literal",
    $propdoc: {
        elements: "[AST_Node*] array of elements"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            var elements = this.elements;
            for (var i = 0, len = elements.length; i < len; i++) {
                elements[i]._walk(visitor);
            }
        });
    }
});

var AST_Object = DEFNODE("Object", "properties", {
    $documentation: "An object literal",
    $propdoc: {
        properties: "[AST_ObjectProperty*] array of properties"
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            var properties = this.properties;
            for (var i = 0, len = properties.length; i < len; i++) {
                properties[i]._walk(visitor);
            }
        });
    }
});

var AST_ObjectProperty = DEFNODE("ObjectProperty", "key value", {
    $documentation: "Base class for literal object properties",
    $propdoc: {
        key: "[string|AST_Node] property name. For ObjectKeyVal this is a string. For getters, setters and computed property this is an AST_Node.",
        value: "[AST_Node] property value.  For getters and setters this is an AST_Accessor."
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.key instanceof AST_Node)
                this.key._walk(visitor);
            this.value._walk(visitor);
        });
    }
});

var AST_ObjectKeyVal = DEFNODE("ObjectKeyVal", "quote", {
    $documentation: "A key: value object property",
    $propdoc: {
        quote: "[string] the original quote character"
    }
}, AST_ObjectProperty);

var AST_ObjectSetter = DEFNODE("ObjectSetter", "quote static", {
    $propdoc: {
        quote: "[string|undefined] the original quote character, if any",
        static: "[boolean] whether this is a static setter (classes only)"
    },
    $documentation: "An object setter property",
}, AST_ObjectProperty);

var AST_ObjectGetter = DEFNODE("ObjectGetter", "quote static", {
    $propdoc: {
        quote: "[string|undefined] the original quote character, if any",
        static: "[boolean] whether this is a static getter (classes only)"
    },
    $documentation: "An object getter property",
}, AST_ObjectProperty);

var AST_ConciseMethod = DEFNODE("ConciseMethod", "quote static is_generator async", {
    $propdoc: {
        quote: "[string|undefined] the original quote character, if any",
        static: "[boolean] is this method static (classes only)",
        is_generator: "[boolean] is this a generator method",
        async: "[boolean] is this method async",
    },
    $documentation: "An ES6 concise method inside an object or class"
}, AST_ObjectProperty);

var AST_Class = DEFNODE("Class", "name extends properties inlined", {
    $propdoc: {
        name: "[AST_SymbolClass|AST_SymbolDefClass?] optional class name.",
        extends: "[AST_Node]? optional parent class",
        properties: "[AST_ObjectProperty*] array of properties"
    },
    $documentation: "An ES6 class",
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            if (this.name) {
                this.name._walk(visitor);
            }
            if (this.extends) {
                this.extends._walk(visitor);
            }
            this.properties.forEach(function(prop) {
                prop._walk(visitor);
            });
        });
    },
}, AST_Scope);

var AST_DefClass = DEFNODE("DefClass", null, {
    $documentation: "A class definition",
}, AST_Class);

var AST_ClassExpression = DEFNODE("ClassExpression", null, {
    $documentation: "A class expression."
}, AST_Class);

var AST_Symbol = DEFNODE("Symbol", "scope name thedef", {
    $propdoc: {
        name: "[string] name of this symbol",
        scope: "[AST_Scope/S] the current scope (not necessarily the definition scope)",
        thedef: "[SymbolDef/S] the definition of this symbol"
    },
    $documentation: "Base class for all symbols"
});

var AST_NewTarget = DEFNODE("NewTarget", null, {
    $documentation: "A reference to new.target"
});

var AST_SymbolDeclaration = DEFNODE("SymbolDeclaration", "init", {
    $documentation: "A declaration symbol (symbol in var/const, function name or argument, symbol in catch)",
}, AST_Symbol);

var AST_SymbolVar = DEFNODE("SymbolVar", null, {
    $documentation: "Symbol defining a variable",
}, AST_SymbolDeclaration);

var AST_SymbolBlockDeclaration = DEFNODE("SymbolBlockDeclaration", null, {
    $documentation: "Base class for block-scoped declaration symbols"
}, AST_SymbolDeclaration);

var AST_SymbolConst = DEFNODE("SymbolConst", null, {
    $documentation: "A constant declaration"
}, AST_SymbolBlockDeclaration);

var AST_SymbolLet = DEFNODE("SymbolLet", null, {
    $documentation: "A block-scoped `let` declaration"
}, AST_SymbolBlockDeclaration);

var AST_SymbolFunarg = DEFNODE("SymbolFunarg", null, {
    $documentation: "Symbol naming a function argument",
}, AST_SymbolVar);

var AST_SymbolDefun = DEFNODE("SymbolDefun", null, {
    $documentation: "Symbol defining a function",
}, AST_SymbolDeclaration);

var AST_SymbolMethod = DEFNODE("SymbolMethod", null, {
    $documentation: "Symbol in an object defining a method",
}, AST_Symbol);

var AST_SymbolLambda = DEFNODE("SymbolLambda", null, {
    $documentation: "Symbol naming a function expression",
}, AST_SymbolDeclaration);

var AST_SymbolDefClass = DEFNODE("SymbolDefClass", null, {
    $documentation: "Symbol naming a class's name in a class declaration. Lexically scoped to its containing scope, and accessible within the class."
}, AST_SymbolBlockDeclaration);

var AST_SymbolClass = DEFNODE("SymbolClass", null, {
    $documentation: "Symbol naming a class's name. Lexically scoped to the class."
}, AST_SymbolDeclaration);

var AST_SymbolCatch = DEFNODE("SymbolCatch", null, {
    $documentation: "Symbol naming the exception in catch",
}, AST_SymbolBlockDeclaration);

var AST_SymbolImport = DEFNODE("SymbolImport", null, {
    $documentation: "Symbol referring to an imported name",
}, AST_SymbolBlockDeclaration);

var AST_SymbolImportForeign = DEFNODE("SymbolImportForeign", null, {
    $documentation: "A symbol imported from a module, but it is defined in the other module, and its real name is irrelevant for this module's purposes",
}, AST_Symbol);

var AST_Label = DEFNODE("Label", "references", {
    $documentation: "Symbol naming a label (declaration)",
    $propdoc: {
        references: "[AST_LoopControl*] a list of nodes referring to this label"
    },
    initialize: function() {
        this.references = [];
        this.thedef = this;
    }
}, AST_Symbol);

var AST_SymbolRef = DEFNODE("SymbolRef", null, {
    $documentation: "Reference to some symbol (not definition/declaration)",
}, AST_Symbol);

var AST_SymbolExport = DEFNODE("SymbolExport", null, {
    $documentation: "Symbol referring to a name to export",
}, AST_SymbolRef);

var AST_SymbolExportForeign = DEFNODE("SymbolExportForeign", null, {
    $documentation: "A symbol exported from this module, but it is used in the other module, and its real name is irrelevant for this module's purposes",
}, AST_Symbol);

var AST_LabelRef = DEFNODE("LabelRef", null, {
    $documentation: "Reference to a label symbol",
}, AST_Symbol);

var AST_This = DEFNODE("This", null, {
    $documentation: "The `this` symbol",
}, AST_Symbol);

var AST_Super = DEFNODE("Super", null, {
    $documentation: "The `super` symbol",
}, AST_This);

var AST_Constant = DEFNODE("Constant", null, {
    $documentation: "Base class for all constants",
    getValue: function() {
        return this.value;
    }
});

var AST_String = DEFNODE("String", "value quote", {
    $documentation: "A string literal",
    $propdoc: {
        value: "[string] the contents of this string",
        quote: "[string] the original quote character"
    }
}, AST_Constant);

var AST_Number = DEFNODE("Number", "value literal", {
    $documentation: "A number literal",
    $propdoc: {
        value: "[number] the numeric value",
        literal: "[string] numeric value as string (optional)"
    }
}, AST_Constant);

var AST_RegExp = DEFNODE("RegExp", "value", {
    $documentation: "A regexp literal",
    $propdoc: {
        value: "[RegExp] the actual regexp",
    }
}, AST_Constant);

var AST_Atom = DEFNODE("Atom", null, {
    $documentation: "Base class for atoms",
}, AST_Constant);

var AST_Null = DEFNODE("Null", null, {
    $documentation: "The `null` atom",
    value: null
}, AST_Atom);

var AST_NaN = DEFNODE("NaN", null, {
    $documentation: "The impossible value",
    value: 0/0
}, AST_Atom);

var AST_Undefined = DEFNODE("Undefined", null, {
    $documentation: "The `undefined` value",
    value: (function() {}())
}, AST_Atom);

var AST_Hole = DEFNODE("Hole", null, {
    $documentation: "A hole in an array",
    value: (function() {}())
}, AST_Atom);

var AST_Infinity = DEFNODE("Infinity", null, {
    $documentation: "The `Infinity` value",
    value: 1/0
}, AST_Atom);

var AST_Boolean = DEFNODE("Boolean", null, {
    $documentation: "Base class for booleans",
}, AST_Atom);

var AST_False = DEFNODE("False", null, {
    $documentation: "The `false` atom",
    value: false
}, AST_Boolean);

var AST_True = DEFNODE("True", null, {
    $documentation: "The `true` atom",
    value: true
}, AST_Boolean);

var AST_Await = DEFNODE("Await", "expression", {
    $documentation: "An `await` statement",
    $propdoc: {
        expression: "[AST_Node] the mandatory expression being awaited",
    },
    _walk: function(visitor) {
        return visitor._visit(this, function() {
            this.expression._walk(visitor);
        });
    }
});

var AST_Yield = DEFNODE("Yield", "expression is_star", {
    $documentation: "A `yield` statement",
    $propdoc: {
        expression: "[AST_Node?] the value returned or thrown by this statement; could be null (representing undefined) but only when is_star is set to false",
        is_star: "[Boolean] Whether this is a yield or yield* statement"
    },
    _walk: function(visitor) {
        return visitor._visit(this, this.expression && function() {
            this.expression._walk(visitor);
        });
    }
});

/* -----[ TreeWalker ]----- */

function TreeWalker(callback) {
    this.visit = callback;
    this.stack = [];
    this.directives = Object.create(null);
}
TreeWalker.prototype = {
    _visit: function(node, descend) {
        this.push(node);
        var ret = this.visit(node, descend ? function() {
            descend.call(node);
        } : noop);
        if (!ret && descend) {
            descend.call(node);
        }
        this.pop();
        return ret;
    },
    parent: function(n) {
        return this.stack[this.stack.length - 2 - (n || 0)];
    },
    push: function(node) {
        if (node instanceof AST_Lambda) {
            this.directives = Object.create(this.directives);
        } else if (node instanceof AST_Directive && !this.directives[node.value]) {
            this.directives[node.value] = node;
        } else if (node instanceof AST_Class) {
            this.directives = Object.create(this.directives);
            if (!this.directives["use strict"]) {
                this.directives["use strict"] = node;
            }
        }
        this.stack.push(node);
    },
    pop: function() {
        var node = this.stack.pop();
        if (node instanceof AST_Lambda || node instanceof AST_Class) {
            this.directives = Object.getPrototypeOf(this.directives);
        }
    },
    self: function() {
        return this.stack[this.stack.length - 1];
    },
    find_parent: function(type) {
        var stack = this.stack;
        for (var i = stack.length; --i >= 0;) {
            var x = stack[i];
            if (x instanceof type) return x;
        }
    },
    has_directive: function(type) {
        var dir = this.directives[type];
        if (dir) return dir;
        var node = this.stack[this.stack.length - 1];
        if (node instanceof AST_Scope && node.body) {
            for (var i = 0; i < node.body.length; ++i) {
                var st = node.body[i];
                if (!(st instanceof AST_Directive)) break;
                if (st.value == type) return st;
            }
        }
    },
    loopcontrol_target: function(node) {
        var stack = this.stack;
        if (node.label) for (var i = stack.length; --i >= 0;) {
            var x = stack[i];
            if (x instanceof AST_LabeledStatement && x.label.name == node.label.name)
                return x.body;
        } else for (var i = stack.length; --i >= 0;) {
            var x = stack[i];
            if (x instanceof AST_IterationStatement
                || node instanceof AST_Break && x instanceof AST_Switch)
                return x;
        }
    }
};

// XXX Emscripten: export TreeWalker for walking through AST in acorn-optimizer.js.
exports.TreeWalker = TreeWalker;

/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>
    Parser based on parse-js (http://marijn.haverbeke.nl/parse-js/).

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

var KEYWORDS = "break case catch class const continue debugger default delete do else export extends finally for function if in instanceof let new return switch throw try typeof var void while with";
var KEYWORDS_ATOM = "false null true";
var RESERVED_WORDS = "enum implements import interface package private protected public static super this " + KEYWORDS_ATOM + " " + KEYWORDS;
var KEYWORDS_BEFORE_EXPRESSION = "return new delete throw else case yield await";

KEYWORDS = makePredicate(KEYWORDS);
RESERVED_WORDS = makePredicate(RESERVED_WORDS);
KEYWORDS_BEFORE_EXPRESSION = makePredicate(KEYWORDS_BEFORE_EXPRESSION);
KEYWORDS_ATOM = makePredicate(KEYWORDS_ATOM);

var OPERATOR_CHARS = makePredicate(characters("+-*&%=<>!?|~^"));

var RE_NUM_LITERAL = /[0-9a-f]/i;
var RE_HEX_NUMBER = /^0x[0-9a-f]+$/i;
var RE_OCT_NUMBER = /^0[0-7]+$/;
var RE_ES6_OCT_NUMBER = /^0o[0-7]+$/i;
var RE_BIN_NUMBER = /^0b[01]+$/i;
var RE_DEC_NUMBER = /^\d*\.?\d*(?:e[+-]?\d*(?:\d\.?|\.?\d)\d*)?$/i;

var OPERATORS = makePredicate([
    "in",
    "instanceof",
    "typeof",
    "new",
    "void",
    "delete",
    "++",
    "--",
    "+",
    "-",
    "!",
    "~",
    "&",
    "|",
    "^",
    "*",
    "**",
    "/",
    "%",
    ">>",
    "<<",
    ">>>",
    "<",
    ">",
    "<=",
    ">=",
    "==",
    "===",
    "!=",
    "!==",
    "?",
    "=",
    "+=",
    "-=",
    "/=",
    "*=",
    "**=",
    "%=",
    ">>=",
    "<<=",
    ">>>=",
    "|=",
    "^=",
    "&=",
    "&&",
    "||"
]);

var WHITESPACE_CHARS = makePredicate(characters(" \u00a0\n\r\t\f\u000b\u200b\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u2028\u2029\u202f\u205f\u3000\uFEFF"));

var NEWLINE_CHARS = makePredicate(characters("\n\r\u2028\u2029"));

var PUNC_AFTER_EXPRESSION = makePredicate(characters(";]),:"));

var PUNC_BEFORE_EXPRESSION = makePredicate(characters("[{(,;:"));

var PUNC_CHARS = makePredicate(characters("[]{}(),;:"));

/* -----[ Tokenizer ]----- */

// surrogate safe regexps adapted from https://github.com/mathiasbynens/unicode-8.0.0/tree/89b412d8a71ecca9ed593d9e9fa073ab64acfebe/Binary_Property
var UNICODE = {
    ID_Start: /[A-Za-z\xAA\xB5\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u05D0-\u05EA\u05F0-\u05F2\u0620-\u064A\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE\u06EF\u06FA-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07CA-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u08A0-\u08B4\u0904-\u0939\u093D\u0950\u0958-\u0961\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09F0\u09F1\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B71\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C60\u0C61\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDE\u0CE0\u0CE1\u0CF1\u0CF2\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D5F-\u0D61\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0EDC-\u0EDF\u0F00\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1711\u1720-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u1820-\u1877\u1880-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1950-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u1A00-\u1A16\u1A20-\u1A54\u1AA7\u1B05-\u1B33\u1B45-\u1B4B\u1B83-\u1BA0\u1BAE\u1BAF\u1BBA-\u1BE5\u1C00-\u1C23\u1C4D-\u1C4F\u1C5A-\u1C7D\u1CE9-\u1CEC\u1CEE-\u1CF1\u1CF5\u1CF6\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2071\u207F\u2090-\u209C\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309B-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FD5\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA61F\uA62A\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7AD\uA7B0-\uA7B7\uA7F7-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA840-\uA873\uA882-\uA8B3\uA8F2-\uA8F7\uA8FB\uA8FD\uA90A-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF\uA9E0-\uA9E4\uA9E6-\uA9EF\uA9FA-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB65\uAB70-\uABE2\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDE80-\uDE9C\uDEA0-\uDED0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE4\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2]|\uD804[\uDC03-\uDC37\uDC83-\uDCAF\uDCD0-\uDCE8\uDD03-\uDD26\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE2B\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61]|\uD805[\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE80-\uDEAA\uDF00-\uDF19]|\uD806[\uDCA0-\uDCDF\uDCFF\uDEC0-\uDEF8]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50\uDF93-\uDF9F]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB]|\uD83A[\uDC00-\uDCC4]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1]|\uD87E[\uDC00-\uDE1D]/,
    ID_Continue: /[0-9A-Z_a-z\xAA\xB5\xB7\xBA\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0300-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u0483-\u0487\u048A-\u052F\u0531-\u0556\u0559\u0561-\u0587\u0591-\u05BD\u05BF\u05C1\u05C2\u05C4\u05C5\u05C7\u05D0-\u05EA\u05F0-\u05F2\u0610-\u061A\u0620-\u0669\u066E-\u06D3\u06D5-\u06DC\u06DF-\u06E8\u06EA-\u06FC\u06FF\u0710-\u074A\u074D-\u07B1\u07C0-\u07F5\u07FA\u0800-\u082D\u0840-\u085B\u08A0-\u08B4\u08E3-\u0963\u0966-\u096F\u0971-\u0983\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BC-\u09C4\u09C7\u09C8\u09CB-\u09CE\u09D7\u09DC\u09DD\u09DF-\u09E3\u09E6-\u09F1\u0A01-\u0A03\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A3C\u0A3E-\u0A42\u0A47\u0A48\u0A4B-\u0A4D\u0A51\u0A59-\u0A5C\u0A5E\u0A66-\u0A75\u0A81-\u0A83\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABC-\u0AC5\u0AC7-\u0AC9\u0ACB-\u0ACD\u0AD0\u0AE0-\u0AE3\u0AE6-\u0AEF\u0AF9\u0B01-\u0B03\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3C-\u0B44\u0B47\u0B48\u0B4B-\u0B4D\u0B56\u0B57\u0B5C\u0B5D\u0B5F-\u0B63\u0B66-\u0B6F\u0B71\u0B82\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BBE-\u0BC2\u0BC6-\u0BC8\u0BCA-\u0BCD\u0BD0\u0BD7\u0BE6-\u0BEF\u0C00-\u0C03\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D-\u0C44\u0C46-\u0C48\u0C4A-\u0C4D\u0C55\u0C56\u0C58-\u0C5A\u0C60-\u0C63\u0C66-\u0C6F\u0C81-\u0C83\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBC-\u0CC4\u0CC6-\u0CC8\u0CCA-\u0CCD\u0CD5\u0CD6\u0CDE\u0CE0-\u0CE3\u0CE6-\u0CEF\u0CF1\u0CF2\u0D01-\u0D03\u0D05-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D-\u0D44\u0D46-\u0D48\u0D4A-\u0D4E\u0D57\u0D5F-\u0D63\u0D66-\u0D6F\u0D7A-\u0D7F\u0D82\u0D83\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DCA\u0DCF-\u0DD4\u0DD6\u0DD8-\u0DDF\u0DE6-\u0DEF\u0DF2\u0DF3\u0E01-\u0E3A\u0E40-\u0E4E\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E87\u0E88\u0E8A\u0E8D\u0E94-\u0E97\u0E99-\u0E9F\u0EA1-\u0EA3\u0EA5\u0EA7\u0EAA\u0EAB\u0EAD-\u0EB9\u0EBB-\u0EBD\u0EC0-\u0EC4\u0EC6\u0EC8-\u0ECD\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F18\u0F19\u0F20-\u0F29\u0F35\u0F37\u0F39\u0F3E-\u0F47\u0F49-\u0F6C\u0F71-\u0F84\u0F86-\u0F97\u0F99-\u0FBC\u0FC6\u1000-\u1049\u1050-\u109D\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u135D-\u135F\u1369-\u1371\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u170C\u170E-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176C\u176E-\u1770\u1772\u1773\u1780-\u17D3\u17D7\u17DC\u17DD\u17E0-\u17E9\u180B-\u180D\u1810-\u1819\u1820-\u1877\u1880-\u18AA\u18B0-\u18F5\u1900-\u191E\u1920-\u192B\u1930-\u193B\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A1B\u1A20-\u1A5E\u1A60-\u1A7C\u1A7F-\u1A89\u1A90-\u1A99\u1AA7\u1AB0-\u1ABD\u1B00-\u1B4B\u1B50-\u1B59\u1B6B-\u1B73\u1B80-\u1BF3\u1C00-\u1C37\u1C40-\u1C49\u1C4D-\u1C7D\u1CD0-\u1CD2\u1CD4-\u1CF6\u1CF8\u1CF9\u1D00-\u1DF5\u1DFC-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u203F\u2040\u2054\u2071\u207F\u2090-\u209C\u20D0-\u20DC\u20E1\u20E5-\u20F0\u2102\u2107\u210A-\u2113\u2115\u2118-\u211D\u2124\u2126\u2128\u212A-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2160-\u2188\u2C00-\u2C2E\u2C30-\u2C5E\u2C60-\u2CE4\u2CEB-\u2CF3\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D7F-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2DE0-\u2DFF\u3005-\u3007\u3021-\u302F\u3031-\u3035\u3038-\u303C\u3041-\u3096\u3099-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312D\u3131-\u318E\u31A0-\u31BA\u31F0-\u31FF\u3400-\u4DB5\u4E00-\u9FD5\uA000-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66F\uA674-\uA67D\uA67F-\uA6F1\uA717-\uA71F\uA722-\uA788\uA78B-\uA7AD\uA7B0-\uA7B7\uA7F7-\uA827\uA840-\uA873\uA880-\uA8C4\uA8D0-\uA8D9\uA8E0-\uA8F7\uA8FB\uA8FD\uA900-\uA92D\uA930-\uA953\uA960-\uA97C\uA980-\uA9C0\uA9CF-\uA9D9\uA9E0-\uA9FE\uAA00-\uAA36\uAA40-\uAA4D\uAA50-\uAA59\uAA60-\uAA76\uAA7A-\uAAC2\uAADB-\uAADD\uAAE0-\uAAEF\uAAF2-\uAAF6\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB65\uAB70-\uABEA\uABEC\uABED\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE00-\uFE0F\uFE20-\uFE2F\uFE33\uFE34\uFE4D-\uFE4F\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF3F\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD40-\uDD74\uDDFD\uDE80-\uDE9C\uDEA0-\uDED0\uDEE0\uDF00-\uDF1F\uDF30-\uDF4A\uDF50-\uDF7A\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDD00-\uDD27\uDD30-\uDD63\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC60-\uDC76\uDC80-\uDC9E\uDCE0-\uDCF2\uDCF4\uDCF5\uDD00-\uDD15\uDD20-\uDD39\uDD80-\uDDB7\uDDBE\uDDBF\uDE00-\uDE03\uDE05\uDE06\uDE0C-\uDE13\uDE15-\uDE17\uDE19-\uDE33\uDE38-\uDE3A\uDE3F\uDE60-\uDE7C\uDE80-\uDE9C\uDEC0-\uDEC7\uDEC9-\uDEE6\uDF00-\uDF35\uDF40-\uDF55\uDF60-\uDF72\uDF80-\uDF91]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2]|\uD804[\uDC00-\uDC46\uDC66-\uDC6F\uDC7F-\uDCBA\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD00-\uDD34\uDD36-\uDD3F\uDD50-\uDD73\uDD76\uDD80-\uDDC4\uDDCA-\uDDCC\uDDD0-\uDDDA\uDDDC\uDE00-\uDE11\uDE13-\uDE37\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEEA\uDEF0-\uDEF9\uDF00-\uDF03\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3C-\uDF44\uDF47\uDF48\uDF4B-\uDF4D\uDF50\uDF57\uDF5D-\uDF63\uDF66-\uDF6C\uDF70-\uDF74]|\uD805[\uDC80-\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDB5\uDDB8-\uDDC0\uDDD8-\uDDDD\uDE00-\uDE40\uDE44\uDE50-\uDE59\uDE80-\uDEB7\uDEC0-\uDEC9\uDF00-\uDF19\uDF1D-\uDF2B\uDF30-\uDF39]|\uD806[\uDCA0-\uDCE9\uDCFF\uDEC0-\uDEF8]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6E\uDC80-\uDD43]|[\uD80C\uD840-\uD868\uD86A-\uD86C\uD86F-\uD872][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2E]|\uD811[\uDC00-\uDE46]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDED0-\uDEED\uDEF0-\uDEF4\uDF00-\uDF36\uDF40-\uDF43\uDF50-\uDF59\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDF00-\uDF44\uDF50-\uDF7E\uDF8F-\uDF9F]|\uD82C[\uDC00\uDC01]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99\uDC9D\uDC9E]|\uD834[\uDD65-\uDD69\uDD6D-\uDD72\uDD7B-\uDD82\uDD85-\uDD8B\uDDAA-\uDDAD\uDE42-\uDE44]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA5\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD836[\uDE00-\uDE36\uDE3B-\uDE6C\uDE75\uDE84\uDE9B-\uDE9F\uDEA1-\uDEAF]|\uD83A[\uDC00-\uDCC4\uDCD0-\uDCD6]|\uD83B[\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD869[\uDC00-\uDED6\uDF00-\uDFFF]|\uD86D[\uDC00-\uDF34\uDF40-\uDFFF]|\uD86E[\uDC00-\uDC1D\uDC20-\uDFFF]|\uD873[\uDC00-\uDEA1]|\uD87E[\uDC00-\uDE1D]|\uDB40[\uDD00-\uDDEF]/,
};

function get_full_char(str, pos) {
    var char = str.charAt(pos);
    if (is_surrogate_pair_head(char)) {
        var next = str.charAt(pos + 1);
        if (is_surrogate_pair_tail(next)) {
            return char + next;
        }
    }
    if (is_surrogate_pair_tail(char)) {
        var prev = str.charAt(pos - 1);
        if (is_surrogate_pair_head(prev)) {
            return prev + char;
        }
    }
    return char;
}

function get_full_char_code(str, pos) {
    // https://en.wikipedia.org/wiki/Universal_Character_Set_characters#Surrogates
    if (is_surrogate_pair_head(str.charAt(pos))) {
        return 0x10000 + (str.charCodeAt(pos) - 0xd800 << 10) + str.charCodeAt(pos + 1) - 0xdc00;
    }
    return str.charCodeAt(pos);
}

function get_full_char_length(str) {
    var surrogates = 0;

    for (var i = 0; i < str.length; i++) {
        if (is_surrogate_pair_head(str.charCodeAt(i))) {
            if (is_surrogate_pair_tail(str.charCodeAt(i + 1))) {
                surrogates++;
                i++;
            }
        }
    }

    return str.length - surrogates;
}

function from_char_code(code) {
    // Based on https://github.com/mathiasbynens/String.fromCodePoint/blob/master/fromcodepoint.js
    if (code > 0xFFFF) {
        code -= 0x10000;
        return (String.fromCharCode((code >> 10) + 0xD800) +
            String.fromCharCode((code % 0x400) + 0xDC00));
    }
    return String.fromCharCode(code);
}

function is_surrogate_pair_head(code) {
    if (typeof code === "string")
        code = code.charCodeAt(0);

    return code >= 0xd800 && code <= 0xdbff;
}

function is_surrogate_pair_tail(code) {
    if (typeof code === "string")
        code = code.charCodeAt(0);
    return code >= 0xdc00 && code <= 0xdfff;
}

function is_digit(code) {
    return code >= 48 && code <= 57;
}

function is_identifier(name) {
    if (typeof name !== "string" || RESERVED_WORDS(name))
        return false;

    return true;
}

function is_identifier_start(ch) {
    var code = ch.charCodeAt(0);
    return UNICODE.ID_Start.test(ch) || code == 36 || code == 95;
}

function is_identifier_char(ch) {
    var code = ch.charCodeAt(0);
    return UNICODE.ID_Continue.test(ch)
        || code == 36
        || code == 95
        || code == 8204 // \u200c: zero-width non-joiner <ZWNJ>
        || code == 8205 // \u200d: zero-width joiner <ZWJ> (in my ECMA-262 PDF, this is also 200c)
    ;
}

function is_identifier_string(str) {
    return /^[a-z_$][a-z0-9_$]*$/i.test(str);
}

function parse_js_number(num) {
    if (RE_HEX_NUMBER.test(num)) {
        return parseInt(num.substr(2), 16);
    } else if (RE_OCT_NUMBER.test(num)) {
        return parseInt(num.substr(1), 8);
    } else if (RE_ES6_OCT_NUMBER.test(num)) {
        return parseInt(num.substr(2), 8);
    } else if (RE_BIN_NUMBER.test(num)) {
        return parseInt(num.substr(2), 2);
    } else if (RE_DEC_NUMBER.test(num)) {
        return parseFloat(num);
    } else {
        var val = parseFloat(num);
        if (val == num) return val;
    }
}

function JS_Parse_Error(message, filename, line, col, pos) {
    this.message = message;
    this.filename = filename;
    this.line = line;
    this.col = col;
    this.pos = pos;
}
JS_Parse_Error.prototype = Object.create(Error.prototype);
JS_Parse_Error.prototype.constructor = JS_Parse_Error;
JS_Parse_Error.prototype.name = "SyntaxError";
configure_error_stack(JS_Parse_Error);

function js_error(message, filename, line, col, pos) {
    throw new JS_Parse_Error(message, filename, line, col, pos);
}

function is_token(token, type, val) {
    return token.type == type && (val == null || token.value == val);
}

var EX_EOF = {};

function tokenizer($TEXT, filename, html5_comments, shebang) {

    var S = {
        text            : $TEXT,
        filename        : filename,
        pos             : 0,
        tokpos          : 0,
        line            : 1,
        tokline         : 0,
        col             : 0,
        tokcol          : 0,
        newline_before  : false,
        regex_allowed   : false,
        brace_counter   : 0,
        template_braces : [],
        comments_before : [],
        directives      : {},
        directive_stack : []
    };

    function peek() { return get_full_char(S.text, S.pos); }

    function next(signal_eof, in_string) {
        var ch = get_full_char(S.text, S.pos++);
        if (signal_eof && !ch)
            throw EX_EOF;
        if (NEWLINE_CHARS(ch)) {
            S.newline_before = S.newline_before || !in_string;
            ++S.line;
            S.col = 0;
            if (!in_string && ch == "\r" && peek() == "\n") {
                // treat a \r\n sequence as a single \n
                ++S.pos;
                ch = "\n";
            }
        } else {
            if (ch.length > 1) {
                ++S.pos;
                ++S.col;
            }
            ++S.col;
        }
        return ch;
    }

    function forward(i) {
        while (i-- > 0) next();
    }

    function looking_at(str) {
        return S.text.substr(S.pos, str.length) == str;
    }

    function find_eol() {
        var text = S.text;
        for (var i = S.pos, n = S.text.length; i < n; ++i) {
            var ch = text[i];
            if (NEWLINE_CHARS(ch))
                return i;
        }
        return -1;
    }

    function find(what, signal_eof) {
        var pos = S.text.indexOf(what, S.pos);
        if (signal_eof && pos == -1) throw EX_EOF;
        return pos;
    }

    function start_token() {
        S.tokline = S.line;
        S.tokcol = S.col;
        S.tokpos = S.pos;
    }

    var prev_was_dot = false;
    var previous_token = null;
    function token(type, value, is_comment) {
        S.regex_allowed = ((type == "operator" && !UNARY_POSTFIX(value)) ||
                           (type == "keyword" && KEYWORDS_BEFORE_EXPRESSION(value)) ||
                           (type == "punc" && PUNC_BEFORE_EXPRESSION(value))) ||
                           (type == "arrow");
        if (type == "punc" && value == ".") {
            prev_was_dot = true;
        } else if (!is_comment) {
            prev_was_dot = false;
        }
        var ret = {
            type    : type,
            value   : value,
            line    : S.tokline,
            col     : S.tokcol,
            pos     : S.tokpos,
            endline : S.line,
            endcol  : S.col,
            endpos  : S.pos,
            nlb     : S.newline_before,
            file    : filename
        };
        if (/^(?:num|string|regexp)$/i.test(type)) {
            ret.raw = $TEXT.substring(ret.pos, ret.endpos);
        }
        if (!is_comment) {
            ret.comments_before = S.comments_before;
            ret.comments_after = S.comments_before = [];
        }
        S.newline_before = false;
        ret = new AST_Token(ret);
        if (!is_comment) previous_token = ret;
        return ret;
    }

    function skip_whitespace() {
        while (WHITESPACE_CHARS(peek()))
            next();
    }

    function read_while(pred) {
        var ret = "", ch, i = 0;
        while ((ch = peek()) && pred(ch, i++))
            ret += next();
        return ret;
    }

    function parse_error(err) {
        js_error(err, filename, S.tokline, S.tokcol, S.tokpos);
    }

    function read_num(prefix) {
        var has_e = false, after_e = false, has_x = false, has_dot = prefix == ".";
        var num = read_while(function(ch, i) {
            var code = ch.charCodeAt(0);
            switch (code) {
              case 98: case 66: // bB
                return (has_x = true); // Can occur in hex sequence, don't return false yet
              case 111: case 79: // oO
              case 120: case 88: // xX
                return has_x ? false : (has_x = true);
              case 101: case 69: // eE
                return has_x ? true : has_e ? false : (has_e = after_e = true);
              case 45: // -
                return after_e || (i == 0 && !prefix);
              case 43: // +
                return after_e;
              case (after_e = false, 46): // .
                return (!has_dot && !has_x && !has_e) ? (has_dot = true) : false;
            }
            return RE_NUM_LITERAL.test(ch);
        });
        if (prefix) num = prefix + num;
        if (RE_OCT_NUMBER.test(num) && next_token.has_directive("use strict")) {
            parse_error("Legacy octal literals are not allowed in strict mode");
        }
        var valid = parse_js_number(num);
        if (!isNaN(valid)) {
            return token("num", valid);
        } else {
            parse_error("Invalid syntax: " + num);
        }
    }

    function read_escaped_char(in_string, strict_hex, template_string) {
        var ch = next(true, in_string);
        switch (ch.charCodeAt(0)) {
          case 110 : return "\n";
          case 114 : return "\r";
          case 116 : return "\t";
          case 98  : return "\b";
          case 118 : return "\u000b"; // \v
          case 102 : return "\f";
          case 120 : return String.fromCharCode(hex_bytes(2, strict_hex)); // \x
          case 117 : // \u
            if (peek() == "{") {
                next(true);
                if (peek() === "}")
                    parse_error("Expecting hex-character between {}");
                while (peek() == "0") next(true); // No significance
                var result, length = find("}", true) - S.pos;
                // Avoid 32 bit integer overflow (1 << 32 === 1)
                // We know first character isn't 0 and thus out of range anyway
                if (length > 6 || (result = hex_bytes(length, strict_hex)) > 0x10FFFF) {
                    parse_error("Unicode reference out of bounds");
                }
                next(true);
                return from_char_code(result);
            }
            return String.fromCharCode(hex_bytes(4, strict_hex));
          case 10  : return ""; // newline
          case 13  :            // \r
            if (peek() == "\n") { // DOS newline
                next(true, in_string);
                return "";
            }
        }
        if (ch >= "0" && ch <= "7") {
            if (template_string && strict_hex) {
                parse_error("Octal escape sequences are not allowed in template strings");
            }
            return read_octal_escape_sequence(ch, strict_hex);
        }
        return ch;
    }

    function read_octal_escape_sequence(ch, strict_octal) {
        // Read
        var p = peek();
        if (p >= "0" && p <= "7") {
            ch += next(true);
            if (ch[0] <= "3" && (p = peek()) >= "0" && p <= "7")
                ch += next(true);
        }

        // Parse
        if (ch === "0") return "\0";
        if (ch.length > 0 && next_token.has_directive("use strict") && strict_octal)
            parse_error("Legacy octal escape sequences are not allowed in strict mode");
        return String.fromCharCode(parseInt(ch, 8));
    }

    function hex_bytes(n, strict_hex) {
        var num = 0;
        for (; n > 0; --n) {
            if (!strict_hex && isNaN(parseInt(peek(), 16))) {
                return parseInt(num, 16) || "";
            }
            var digit = next(true);
            if (isNaN(parseInt(digit, 16)))
                parse_error("Invalid hex-character pattern in string");
            num += digit;
        }
        return parseInt(num, 16);
    }

    var read_string = with_eof_error("Unterminated string constant", function(quote_char) {
        var quote = next(), ret = "";
        for (;;) {
            var ch = next(true, true);
            if (ch == "\\") ch = read_escaped_char(true, true);
            else if (NEWLINE_CHARS(ch)) parse_error("Unterminated string constant");
            else if (ch == quote) break;
            ret += ch;
        }
        var tok = token("string", ret);
        tok.quote = quote_char;
        return tok;
    });

    var read_template_characters = with_eof_error("Unterminated template", function(begin) {
        if (begin) {
            S.template_braces.push(S.brace_counter);
        }
        var content = "", raw = "", ch, tok;
        next(true, true);
        while ((ch = next(true, true)) != "`") {
            if (ch == "\r") {
                if (peek() == "\n") ++S.pos;
                ch = "\n";
            } else if (ch == "$" && peek() == "{") {
                next(true, true);
                S.brace_counter++;
                tok = token(begin ? "template_head" : "template_substitution", content);
                tok.begin = begin;
                tok.raw = raw;
                tok.end = false;
                return tok;
            }

            raw += ch;
            if (ch == "\\") {
                var tmp = S.pos;
                var prev_is_tag = previous_token.type === "name" || previous_token.type === "punc" && (previous_token.value === ")" || previous_token.value === "]");
                ch = read_escaped_char(true, !prev_is_tag, true);
                raw += S.text.substr(tmp, S.pos - tmp);
            }

            content += ch;
        }
        S.template_braces.pop();
        tok = token(begin ? "template_head" : "template_substitution", content);
        tok.begin = begin;
        tok.raw = raw;
        tok.end = true;
        return tok;
    });

    function skip_line_comment(type) {
        var regex_allowed = S.regex_allowed;
        var i = find_eol(), ret;
        if (i == -1) {
            ret = S.text.substr(S.pos);
            S.pos = S.text.length;
        } else {
            ret = S.text.substring(S.pos, i);
            S.pos = i;
        }
        S.col = S.tokcol + (S.pos - S.tokpos);
        S.comments_before.push(token(type, ret, true));
        S.regex_allowed = regex_allowed;
        return next_token;
    }

    var skip_multiline_comment = with_eof_error("Unterminated multiline comment", function() {
        var regex_allowed = S.regex_allowed;
        var i = find("*/", true);
        var text = S.text.substring(S.pos, i).replace(/\r\n|\r|\u2028|\u2029/g, "\n");
        // update stream position
        forward(get_full_char_length(text) /* text length doesn't count \r\n as 2 char while S.pos - i does */ + 2);
        S.comments_before.push(token("comment2", text, true));
        S.newline_before = S.newline_before || text.indexOf("\n") >= 0;
        S.regex_allowed = regex_allowed;
        return next_token;
    });

    var read_name = with_eof_error("Unterminated identifier name", function() {
        var name = "", ch, escaped = false, hex;
        var read_escaped_identifier_char = function() {
            escaped = true;
            next();
            if (peek() !== "u") {
                parse_error("Expecting UnicodeEscapeSequence -- uXXXX or u{XXXX}");
            }
            return read_escaped_char(false, true);
        };

        // Read first character (ID_Start)
        if ((name = peek()) === "\\") {
            name = read_escaped_identifier_char();
            if (!is_identifier_start(name)) {
                parse_error("First identifier char is an invalid identifier char");
            }
        } else if (is_identifier_start(name)) {
            next();
        } else {
            return "";
        }

        // Read ID_Continue
        while ((ch = peek()) != null) {
            if ((ch = peek()) === "\\") {
                ch = read_escaped_identifier_char();
                if (!is_identifier_char(ch)) {
                    parse_error("Invalid escaped identifier char");
                }
            } else {
                if (!is_identifier_char(ch)) {
                    break;
                }
                next();
            }
            name += ch;
        }
        if (RESERVED_WORDS(name) && escaped) {
            parse_error("Escaped characters are not allowed in keywords");
        }
        return name;
    });

    var read_regexp = with_eof_error("Unterminated regular expression", function(source) {
        var prev_backslash = false, ch, in_class = false;
        while ((ch = next(true))) if (NEWLINE_CHARS(ch)) {
            parse_error("Unexpected line terminator");
        } else if (prev_backslash) {
            source += "\\" + ch;
            prev_backslash = false;
        } else if (ch == "[") {
            in_class = true;
            source += ch;
        } else if (ch == "]" && in_class) {
            in_class = false;
            source += ch;
        } else if (ch == "/" && !in_class) {
            break;
        } else if (ch == "\\") {
            prev_backslash = true;
        } else {
            source += ch;
        }
        var mods = read_name();
        try {
            var regexp = new RegExp(source, mods);
            regexp.raw_source = "/" + source + "/" + mods;
            return token("regexp", regexp);
        } catch(e) {
            parse_error(e.message);
        }
    });

    function read_operator(prefix) {
        function grow(op) {
            if (!peek()) return op;
            var bigger = op + peek();
            if (OPERATORS(bigger)) {
                next();
                return grow(bigger);
            } else {
                return op;
            }
        }
        return token("operator", grow(prefix || next()));
    }

    function handle_slash() {
        next();
        switch (peek()) {
          case "/":
            next();
            return skip_line_comment("comment1");
          case "*":
            next();
            return skip_multiline_comment();
        }
        return S.regex_allowed ? read_regexp("") : read_operator("/");
    }

    function handle_eq_sign() {
        next();
        if (peek() === ">") {
            next();
            return token("arrow", "=>");
        } else {
            return read_operator("=");
        }
    }

    function handle_dot() {
        next();
        if (is_digit(peek().charCodeAt(0))) {
            return read_num(".");
        }
        if (peek() === ".") {
            next();  // Consume second dot
            next();  // Consume third dot
            return token("expand", "...");
        }

        return token("punc", ".");
    }

    function read_word() {
        var word = read_name();
        if (prev_was_dot) return token("name", word);
        return KEYWORDS_ATOM(word) ? token("atom", word)
            : !KEYWORDS(word) ? token("name", word)
            : OPERATORS(word) ? token("operator", word)
            : token("keyword", word);
    }

    function with_eof_error(eof_error, cont) {
        return function(x) {
            try {
                return cont(x);
            } catch(ex) {
                if (ex === EX_EOF) parse_error(eof_error);
                else throw ex;
            }
        };
    }

    function next_token(force_regexp) {
        if (force_regexp != null)
            return read_regexp(force_regexp);
        if (shebang && S.pos == 0 && looking_at("#!")) {
            start_token();
            forward(2);
            skip_line_comment("comment5");
        }
        for (;;) {
            skip_whitespace();
            start_token();
            if (html5_comments) {
                if (looking_at("<!--")) {
                    forward(4);
                    skip_line_comment("comment3");
                    continue;
                }
                if (looking_at("-->") && S.newline_before) {
                    forward(3);
                    skip_line_comment("comment4");
                    continue;
                }
            }
            var ch = peek();
            if (!ch) return token("eof");
            var code = ch.charCodeAt(0);
            switch (code) {
              case 34: case 39: return read_string(ch);
              case 46: return handle_dot();
              case 47: {
                  var tok = handle_slash();
                  if (tok === next_token) continue;
                  return tok;
              }
              case 61: return handle_eq_sign();
              case 96: return read_template_characters(true);
              case 123:
                S.brace_counter++;
                break;
              case 125:
                S.brace_counter--;
                if (S.template_braces.length > 0
                    && S.template_braces[S.template_braces.length - 1] === S.brace_counter)
                    return read_template_characters(false);
                break;
            }
            if (is_digit(code)) return read_num();
            if (PUNC_CHARS(ch)) return token("punc", next());
            if (OPERATOR_CHARS(ch)) return read_operator();
            if (code == 92 || is_identifier_start(ch)) return read_word();
            break;
        }
        parse_error("Unexpected character '" + ch + "'");
    }

    next_token.next = next;
    next_token.peek = peek;

    next_token.context = function(nc) {
        if (nc) S = nc;
        return S;
    };

    next_token.add_directive = function(directive) {
        S.directive_stack[S.directive_stack.length - 1].push(directive);

        if (S.directives[directive] === undefined) {
            S.directives[directive] = 1;
        } else {
            S.directives[directive]++;
        }
    };

    next_token.push_directives_stack = function() {
        S.directive_stack.push([]);
    };

    next_token.pop_directives_stack = function() {
        var directives = S.directive_stack[S.directive_stack.length - 1];

        for (var i = 0; i < directives.length; i++) {
            S.directives[directives[i]]--;
        }

        S.directive_stack.pop();
    };

    next_token.has_directive = function(directive) {
        return S.directives[directive] > 0;
    };

    return next_token;

}

/* -----[ Parser (constants) ]----- */

var UNARY_PREFIX = makePredicate([
    "typeof",
    "void",
    "delete",
    "--",
    "++",
    "!",
    "~",
    "-",
    "+"
]);

var UNARY_POSTFIX = makePredicate([ "--", "++" ]);

var ASSIGNMENT = makePredicate([ "=", "+=", "-=", "/=", "*=", "**=", "%=", ">>=", "<<=", ">>>=", "|=", "^=", "&=" ]);

var PRECEDENCE = (function(a, ret) {
    for (var i = 0; i < a.length; ++i) {
        var b = a[i];
        for (var j = 0; j < b.length; ++j) {
            ret[b[j]] = i + 1;
        }
    }
    return ret;
})(
    [
        ["||"],
        ["&&"],
        ["|"],
        ["^"],
        ["&"],
        ["==", "===", "!=", "!=="],
        ["<", ">", "<=", ">=", "in", "instanceof"],
        [">>", "<<", ">>>"],
        ["+", "-"],
        ["*", "/", "%"],
        ["**"]
    ],
    {}
);

var ATOMIC_START_TOKEN = makePredicate([ "atom", "num", "string", "regexp", "name" ]);

/* -----[ Parser ]----- */

function parse($TEXT, options) {

    options = defaults(options, {
        bare_returns   : false,
        ecma           : 8,
        expression     : false,
        filename       : null,
        html5_comments : true,
        module         : false,
        shebang        : true,
        strict         : false,
        toplevel       : null,
    }, true);

    var S = {
        input         : (typeof $TEXT == "string"
                         ? tokenizer($TEXT, options.filename,
                                     options.html5_comments, options.shebang)
                         : $TEXT),
        token         : null,
        prev          : null,
        peeked        : null,
        in_function   : 0,
        in_async      : -1,
        in_generator  : -1,
        in_directives : true,
        in_loop       : 0,
        labels        : []
    };

    S.token = next();

    function is(type, value) {
        return is_token(S.token, type, value);
    }

    function peek() { return S.peeked || (S.peeked = S.input()); }

    function next() {
        S.prev = S.token;

        if (!S.peeked) peek();
        S.token = S.peeked;
        S.peeked = null;
        S.in_directives = S.in_directives && (
            S.token.type == "string" || is("punc", ";")
        );
        return S.token;
    }

    function prev() {
        return S.prev;
    }

    function croak(msg, line, col, pos) {
        var ctx = S.input.context();
        js_error(msg,
                 ctx.filename,
                 line != null ? line : ctx.tokline,
                 col != null ? col : ctx.tokcol,
                 pos != null ? pos : ctx.tokpos);
    }

    function token_error(token, msg) {
        croak(msg, token.line, token.col);
    }

    function unexpected(token) {
        if (token == null)
            token = S.token;
        token_error(token, "Unexpected token: " + token.type + " (" + token.value + ")");
    }

    function expect_token(type, val) {
        if (is(type, val)) {
            return next();
        }
        token_error(S.token, "Unexpected token " + S.token.type + " «" + S.token.value + "»" + ", expected " + type + " «" + val + "»");
    }

    function expect(punc) { return expect_token("punc", punc); }

    function has_newline_before(token) {
        return token.nlb || !all(token.comments_before, function(comment) {
            return !comment.nlb;
        });
    }

    function can_insert_semicolon() {
        return !options.strict
            && (is("eof") || is("punc", "}") || has_newline_before(S.token));
    }

    function is_in_generator() {
        return S.in_generator === S.in_function;
    }

    function is_in_async() {
        return S.in_async === S.in_function;
    }

    function semicolon(optional) {
        if (is("punc", ";")) next();
        else if (!optional && !can_insert_semicolon()) unexpected();
    }

    function parenthesised() {
        expect("(");
        var exp = expression(true);
        expect(")");
        return exp;
    }

    function embed_tokens(parser) {
        return function() {
            var start = S.token;
            var expr = parser.apply(null, arguments);
            var end = prev();
            expr.start = start;
            expr.end = end;
            return expr;
        };
    }

    function handle_regexp() {
        if (is("operator", "/") || is("operator", "/=")) {
            S.peeked = null;
            S.token = S.input(S.token.value.substr(1)); // force regexp
        }
    }

    var statement = embed_tokens(function(is_export_default, is_for_body, is_if_body) {
        handle_regexp();
        switch (S.token.type) {
          case "string":
            if (S.in_directives) {
                var token = peek();
                if (S.token.raw.indexOf("\\") == -1
                    && (is_token(token, "punc", ";")
                        || is_token(token, "punc", "}")
                        || has_newline_before(token)
                        || is_token(token, "eof"))) {
                    S.input.add_directive(S.token.value);
                } else {
                    S.in_directives = false;
                }
            }
            var dir = S.in_directives, stat = simple_statement();
            return dir && stat.body instanceof AST_String ? new AST_Directive(stat.body) : stat;
          case "template_head":
          case "num":
          case "regexp":
          case "operator":
          case "atom":
            return simple_statement();

          case "name":
            if (S.token.value == "async" && is_token(peek(), "keyword", "function")) {
                next();
                next();
                if (is_for_body) {
                    croak("functions are not allowed as the body of a loop");
                }
                return function_(AST_Defun, false, true, is_export_default);
            }
            if (S.token.value == "import" && !is_token(peek(), "punc", "(")) {
                next();
                var node = import_();
                semicolon();
                return node;
            }
            return is_token(peek(), "punc", ":")
                ? labeled_statement()
                : simple_statement();

          case "punc":
            switch (S.token.value) {
              case "{":
                return new AST_BlockStatement({
                    start : S.token,
                    body  : block_(),
                    end   : prev()
                });
              case "[":
              case "(":
                return simple_statement();
              case ";":
                S.in_directives = false;
                next();
                return new AST_EmptyStatement();
              default:
                unexpected();
            }

          case "keyword":
            switch (S.token.value) {
              case "break":
                next();
                return break_cont(AST_Break);

              case "continue":
                next();
                return break_cont(AST_Continue);

              case "debugger":
                next();
                semicolon();
                return new AST_Debugger();

              case "do":
                next();
                var body = in_loop(statement);
                expect_token("keyword", "while");
                var condition = parenthesised();
                semicolon(true);
                return new AST_Do({
                    body      : body,
                    condition : condition
                });

              case "while":
                next();
                return new AST_While({
                    condition : parenthesised(),
                    body      : in_loop(function() { return statement(false, true); })
                });

              case "for":
                next();
                return for_();

              case "class":
                next();
                if (is_for_body) {
                    croak("classes are not allowed as the body of a loop");
                }
                if (is_if_body) {
                    croak("classes are not allowed as the body of an if");
                }
                return class_(AST_DefClass);

              case "function":
                next();
                if (is_for_body) {
                    croak("functions are not allowed as the body of a loop");
                }
                return function_(AST_Defun, false, false, is_export_default);

              case "if":
                next();
                return if_();

              case "return":
                if (S.in_function == 0 && !options.bare_returns)
                    croak("'return' outside of function");
                next();
                var value = null;
                if (is("punc", ";")) {
                    next();
                } else if (!can_insert_semicolon()) {
                    value = expression(true);
                    semicolon();
                }
                return new AST_Return({
                    value: value
                });

              case "switch":
                next();
                return new AST_Switch({
                    expression : parenthesised(),
                    body       : in_loop(switch_body_)
                });

              case "throw":
                next();
                if (has_newline_before(S.token))
                    croak("Illegal newline after 'throw'");
                var value = expression(true);
                semicolon();
                return new AST_Throw({
                    value: value
                });

              case "try":
                next();
                return try_();

              case "var":
                next();
                var node = var_();
                semicolon();
                return node;

              case "let":
                next();
                var node = let_();
                semicolon();
                return node;

              case "const":
                next();
                var node = const_();
                semicolon();
                return node;

              case "with":
                if (S.input.has_directive("use strict")) {
                    croak("Strict mode may not include a with statement");
                }
                next();
                return new AST_With({
                    expression : parenthesised(),
                    body       : statement()
                });

              case "export":
                if (!is_token(peek(), "punc", "(")) {
                    next();
                    var node = export_();
                    if (is("punc", ";")) semicolon();
                    return node;
                }
            }
        }
        unexpected();
    });

    function labeled_statement() {
        var label = as_symbol(AST_Label);
        if (label.name === "await" && is_in_async()) {
            token_error(S.prev, "await cannot be used as label inside async function");
        }
        if (find_if(function(l) { return l.name == label.name; }, S.labels)) {
            // ECMA-262, 12.12: An ECMAScript program is considered
            // syntactically incorrect if it contains a
            // LabelledStatement that is enclosed by a
            // LabelledStatement with the same Identifier as label.
            croak("Label " + label.name + " defined twice");
        }
        expect(":");
        S.labels.push(label);
        var stat = statement();
        S.labels.pop();
        if (!(stat instanceof AST_IterationStatement)) {
            // check for `continue` that refers to this label.
            // those should be reported as syntax errors.
            // https://github.com/mishoo/UglifyJS2/issues/287
            label.references.forEach(function(ref) {
                if (ref instanceof AST_Continue) {
                    ref = ref.label.start;
                    croak("Continue label `" + label.name + "` refers to non-IterationStatement.",
                          ref.line, ref.col, ref.pos);
                }
            });
        }
        return new AST_LabeledStatement({ body: stat, label: label });
    }

    function simple_statement(tmp) {
        return new AST_SimpleStatement({ body: (tmp = expression(true), semicolon(), tmp) });
    }

    function break_cont(type) {
        var label = null, ldef;
        if (!can_insert_semicolon()) {
            label = as_symbol(AST_LabelRef, true);
        }
        if (label != null) {
            ldef = find_if(function(l) { return l.name == label.name; }, S.labels);
            if (!ldef)
                croak("Undefined label " + label.name);
            label.thedef = ldef;
        } else if (S.in_loop == 0)
            croak(type.TYPE + " not inside a loop or switch");
        semicolon();
        var stat = new type({ label: label });
        if (ldef) ldef.references.push(stat);
        return stat;
    }

    function for_() {
        var for_await_error = "`for await` invalid in this context";
        var await_tok = S.token;
        if (await_tok.type == "name" && await_tok.value == "await") {
            if (!is_in_async()) {
                token_error(await_tok, for_await_error);
            }
            next();
        } else {
            await_tok = false;
        }
        expect("(");
        var init = null;
        if (!is("punc", ";")) {
            init =
                is("keyword", "var") ? (next(), var_(true)) :
                is("keyword", "let") ? (next(), let_(true)) :
                is("keyword", "const") ? (next(), const_(true)) :
                                       expression(true, true);
            var is_in = is("operator", "in");
            var is_of = is("name", "of");
            if (await_tok && !is_of) {
                token_error(await_tok, for_await_error);
            }
            if (is_in || is_of) {
                if (init instanceof AST_Definitions) {
                    if (init.definitions.length > 1)
                        token_error(init.start, "Only one variable declaration allowed in for..in loop");
                } else if (!(is_assignable(init) || (init = to_destructuring(init)) instanceof AST_Destructuring)) {
                    token_error(init.start, "Invalid left-hand side in for..in loop");
                }
                next();
                if (is_in) {
                    return for_in(init);
                } else {
                    return for_of(init, !!await_tok);
                }
            }
        } else if (await_tok) {
            token_error(await_tok, for_await_error);
        }
        return regular_for(init);
    }

    function regular_for(init) {
        expect(";");
        var test = is("punc", ";") ? null : expression(true);
        expect(";");
        var step = is("punc", ")") ? null : expression(true);
        expect(")");
        return new AST_For({
            init      : init,
            condition : test,
            step      : step,
            body      : in_loop(function() { return statement(false, true); })
        });
    }

    function for_of(init, is_await) {
        var lhs = init instanceof AST_Definitions ? init.definitions[0].name : null;
        var obj = expression(true);
        expect(")");
        return new AST_ForOf({
            await  : is_await,
            init   : init,
            name   : lhs,
            object : obj,
            body   : in_loop(function() { return statement(false, true); })
        });
    }

    function for_in(init) {
        var obj = expression(true);
        expect(")");
        return new AST_ForIn({
            init   : init,
            object : obj,
            body   : in_loop(function() { return statement(false, true); })
        });
    }

    var arrow_function = function(start, argnames, is_async) {
        if (has_newline_before(S.token)) {
            croak("Unexpected newline before arrow (=>)");
        }

        expect_token("arrow", "=>");

        var body = _function_body(is("punc", "{"), false, is_async);

        var end =
            body instanceof Array && body.length ? body[body.length - 1].end :
            body instanceof Array ? start :
                body.end;

        return new AST_Arrow({
            start    : start,
            end      : end,
            async    : is_async,
            argnames : argnames,
            body     : body
        });
    };

    var function_ = function(ctor, is_generator_property, is_async, is_export_default) {
        var start = S.token;

        var in_statement = ctor === AST_Defun;
        var is_generator = is("operator", "*");
        if (is_generator) {
            next();
        }

        var name = is("name") ? as_symbol(in_statement ? AST_SymbolDefun : AST_SymbolLambda) : null;
        if (in_statement && !name) {
            if (is_export_default) {
                ctor = AST_Function;
            } else {
                unexpected();
            }
        }

        if (name && ctor !== AST_Accessor && !(name instanceof AST_SymbolDeclaration))
            unexpected(prev());

        var args = [];
        var body = _function_body(true, is_generator || is_generator_property, is_async, name, args);
        return new ctor({
            start : args.start,
            end   : body.end,
            is_generator: is_generator,
            async : is_async,
            name  : name,
            argnames: args,
            body  : body
        });
    };

    function track_used_binding_identifiers(is_parameter, strict) {
        var parameters = {};
        var duplicate = false;
        var default_assignment = false;
        var spread = false;
        var strict_mode = !!strict;
        var tracker = {
            add_parameter: function(token) {
                if (parameters["$" + token.value] !== undefined) {
                    if (duplicate === false) {
                        duplicate = token;
                    }
                    tracker.check_strict();
                } else {
                    parameters["$" + token.value] = true;
                    if (is_parameter) {
                        switch (token.value) {
                          case "arguments":
                          case "eval":
                          case "yield":
                            if (strict_mode) {
                                token_error(token, "Unexpected " + token.value + " identifier as parameter inside strict mode");
                            }
                            break;
                          default:
                            if (RESERVED_WORDS(token.value)) {
                                unexpected();
                            }
                        }
                    }
                }
            },
            mark_default_assignment: function(token) {
                if (default_assignment === false) {
                    default_assignment = token;
                }
            },
            mark_spread: function(token) {
                if (spread === false) {
                    spread = token;
                }
            },
            mark_strict_mode: function() {
                strict_mode = true;
            },
            is_strict: function() {
                return default_assignment !== false || spread !== false || strict_mode;
            },
            check_strict: function() {
                if (tracker.is_strict() && duplicate !== false) {
                    token_error(duplicate, "Parameter " + duplicate.value + " was used already");
                }
            }
        };

        return tracker;
    }

    function parameters(params) {
        var start = S.token;
        var used_parameters = track_used_binding_identifiers(true, S.input.has_directive("use strict"));

        expect("(");

        while (!is("punc", ")")) {
            var param = parameter(used_parameters);
            params.push(param);

            if (!is("punc", ")")) {
                expect(",");
                if (is("punc", ")") && options.ecma < 8) unexpected();
            }

            if (param instanceof AST_Expansion) {
                break;
            }
        }

        next();
    }

    function parameter(used_parameters, symbol_type) {
        var param;
        var expand = false;
        if (used_parameters === undefined) {
            used_parameters = track_used_binding_identifiers(true, S.input.has_directive("use strict"));
        }
        if (is("expand", "...")) {
            expand = S.token;
            used_parameters.mark_spread(S.token);
            next();
        }
        param = binding_element(used_parameters, symbol_type);

        if (is("operator", "=") && expand === false) {
            used_parameters.mark_default_assignment(S.token);
            next();
            param = new AST_DefaultAssign({
                start: param.start,
                left: param,
                operator: "=",
                right: expression(false),
                end: S.token
            });
        }

        if (expand !== false) {
            if (!is("punc", ")")) {
                unexpected();
            }
            param = new AST_Expansion({
                start: expand,
                expression: param,
                end: expand
            });
        }
        used_parameters.check_strict();

        return param;
    }

    function binding_element(used_parameters, symbol_type) {
        var elements = [];
        var first = true;
        var is_expand = false;
        var expand_token;
        var first_token = S.token;
        if (used_parameters === undefined) {
            used_parameters = track_used_binding_identifiers(false, S.input.has_directive("use strict"));
        }
        symbol_type = symbol_type === undefined ? AST_SymbolFunarg : symbol_type;
        if (is("punc", "[")) {
            next();
            while (!is("punc", "]")) {
                if (first) {
                    first = false;
                } else {
                    expect(",");
                }

                if (is("expand", "...")) {
                    is_expand = true;
                    expand_token = S.token;
                    used_parameters.mark_spread(S.token);
                    next();
                }
                if (is("punc")) {
                    switch (S.token.value) {
                      case ",":
                        elements.push(new AST_Hole({
                            start: S.token,
                            end: S.token
                        }));
                        continue;
                      case "]": // Trailing comma after last element
                        break;
                      case "[":
                      case "{":
                        elements.push(binding_element(used_parameters, symbol_type));
                        break;
                      default:
                        unexpected();
                    }
                } else if (is("name")) {
                    used_parameters.add_parameter(S.token);
                    elements.push(as_symbol(symbol_type));
                } else {
                    croak("Invalid function parameter");
                }
                if (is("operator", "=") && is_expand === false) {
                    used_parameters.mark_default_assignment(S.token);
                    next();
                    elements[elements.length - 1] = new AST_DefaultAssign({
                        start: elements[elements.length - 1].start,
                        left: elements[elements.length - 1],
                        operator: "=",
                        right: expression(false),
                        end: S.token
                    });
                }
                if (is_expand) {
                    if (!is("punc", "]")) {
                        croak("Rest element must be last element");
                    }
                    elements[elements.length - 1] = new AST_Expansion({
                        start: expand_token,
                        expression: elements[elements.length - 1],
                        end: expand_token
                    });
                }
            }
            expect("]");
            used_parameters.check_strict();
            return new AST_Destructuring({
                start: first_token,
                names: elements,
                is_array: true,
                end: prev()
            });
        } else if (is("punc", "{")) {
            next();
            while (!is("punc", "}")) {
                if (first) {
                    first = false;
                } else {
                    expect(",");
                }
                if (is("expand", "...")) {
                    is_expand = true;
                    expand_token = S.token;
                    used_parameters.mark_spread(S.token);
                    next();
                }
                if (is("name") && (is_token(peek(), "punc") || is_token(peek(), "operator")) && [",", "}", "="].indexOf(peek().value) !== -1) {
                    used_parameters.add_parameter(S.token);
                    var start = prev();
                    var value = as_symbol(symbol_type);
                    if (is_expand) {
                        elements.push(new AST_Expansion({
                            start: expand_token,
                            expression: value,
                            end: value.end,
                        }));
                    } else {
                        elements.push(new AST_ObjectKeyVal({
                            start: start,
                            key: value.name,
                            value: value,
                            end: value.end,
                        }));
                    }
                } else if (is("punc", "}")) {
                    continue; // Allow trailing hole
                } else {
                    var property_token = S.token;
                    var property = as_property_name();
                    if (property === null) {
                        unexpected(prev());
                    } else if (prev().type === "name" && !is("punc", ":")) {
                        elements.push(new AST_ObjectKeyVal({
                            start: prev(),
                            key: property,
                            value: new symbol_type({
                                start: prev(),
                                name: property,
                                end: prev()
                            }),
                            end: prev()
                        }));
                    } else {
                        expect(":");
                        elements.push(new AST_ObjectKeyVal({
                            start: property_token,
                            quote: property_token.quote,
                            key: property,
                            value: binding_element(used_parameters, symbol_type),
                            end: prev()
                        }));
                    }
                }
                if (is_expand) {
                    if (!is("punc", "}")) {
                        croak("Rest element must be last element");
                    }
                } else if (is("operator", "=")) {
                    used_parameters.mark_default_assignment(S.token);
                    next();
                    elements[elements.length - 1].value = new AST_DefaultAssign({
                        start: elements[elements.length - 1].value.start,
                        left: elements[elements.length - 1].value,
                        operator: "=",
                        right: expression(false),
                        end: S.token
                    });
                }
            }
            expect("}");
            used_parameters.check_strict();
            return new AST_Destructuring({
                start: first_token,
                names: elements,
                is_array: false,
                end: prev()
            });
        } else if (is("name")) {
            used_parameters.add_parameter(S.token);
            return as_symbol(symbol_type);
        } else {
            croak("Invalid function parameter");
        }
    }

    function params_or_seq_(allow_arrows, maybe_sequence) {
        var spread_token;
        var invalid_sequence;
        var trailing_comma;
        var a = [];
        expect("(");
        while (!is("punc", ")")) {
            if (spread_token) unexpected(spread_token);
            if (is("expand", "...")) {
                spread_token = S.token;
                if (maybe_sequence) invalid_sequence = S.token;
                next();
                a.push(new AST_Expansion({
                    start: prev(),
                    expression: expression(),
                    end: S.token,
                }));
            } else {
                a.push(expression());
            }
            if (!is("punc", ")")) {
                expect(",");
                if (is("punc", ")")) {
                    if (options.ecma < 8) unexpected();
                    trailing_comma = prev();
                    if (maybe_sequence) invalid_sequence = trailing_comma;
                }
            }
        }
        expect(")");
        if (allow_arrows && is("arrow", "=>")) {
            if (spread_token && trailing_comma) unexpected(trailing_comma);
        } else if (invalid_sequence) {
            unexpected(invalid_sequence);
        }
        return a;
    }

    function _function_body(block, generator, is_async, name, args) {
        var loop = S.in_loop;
        var labels = S.labels;
        var current_generator = S.in_generator;
        var current_async = S.in_async;
        ++S.in_function;
        if (generator)
            S.in_generator = S.in_function;
        if (is_async)
            S.in_async = S.in_function;
        if (args) parameters(args);
        if (block)
            S.in_directives = true;
        S.in_loop = 0;
        S.labels = [];
        if (block) {
            S.input.push_directives_stack();
            var a = block_();
            if (name) _verify_symbol(name);
            if (args) args.forEach(_verify_symbol);
            S.input.pop_directives_stack();
        } else {
            var a = expression(false);
        }
        --S.in_function;
        S.in_loop = loop;
        S.labels = labels;
        S.in_generator = current_generator;
        S.in_async = current_async;
        return a;
    }

    function _await_expression() {
        // Previous token must be "await" and not be interpreted as an identifier
        if (!is_in_async()) {
            croak("Unexpected await expression outside async function",
                S.prev.line, S.prev.col, S.prev.pos);
        }
        // the await expression is parsed as a unary expression in Babel
        return new AST_Await({
            start: prev(),
            end: S.token,
            expression : maybe_unary(true),
        });
    }

    function _yield_expression() {
        // Previous token must be keyword yield and not be interpret as an identifier
        if (!is_in_generator()) {
            croak("Unexpected yield expression outside generator function",
                S.prev.line, S.prev.col, S.prev.pos);
        }
        var start = S.token;
        var star = false;
        var has_expression = true;

        // Attempt to get expression or star (and then the mandatory expression)
        // behind yield on the same line.
        //
        // If nothing follows on the same line of the yieldExpression,
        // it should default to the value `undefined` for yield to return.
        // In that case, the `undefined` stored as `null` in ast.
        //
        // Note 1: It isn't allowed for yield* to close without an expression
        // Note 2: If there is a nlb between yield and star, it is interpret as
        //         yield <explicit undefined> <inserted automatic semicolon> *
        if (can_insert_semicolon() ||
            (is("punc") && PUNC_AFTER_EXPRESSION(S.token.value))) {
            has_expression = false;

        } else if (is("operator", "*")) {
            star = true;
            next();
        }

        return new AST_Yield({
            start      : start,
            is_star    : star,
            expression : has_expression ? expression() : null,
            end        : prev()
        });
    }

    function if_() {
        var cond = parenthesised(), body = statement(false, false, true), belse = null;
        if (is("keyword", "else")) {
            next();
            belse = statement(false, false, true);
        }
        return new AST_If({
            condition   : cond,
            body        : body,
            alternative : belse
        });
    }

    function block_() {
        expect("{");
        var a = [];
        while (!is("punc", "}")) {
            if (is("eof")) unexpected();
            a.push(statement());
        }
        next();
        return a;
    }

    function switch_body_() {
        expect("{");
        var a = [], cur = null, branch = null, tmp;
        while (!is("punc", "}")) {
            if (is("eof")) unexpected();
            if (is("keyword", "case")) {
                if (branch) branch.end = prev();
                cur = [];
                branch = new AST_Case({
                    start      : (tmp = S.token, next(), tmp),
                    expression : expression(true),
                    body       : cur
                });
                a.push(branch);
                expect(":");
            } else if (is("keyword", "default")) {
                if (branch) branch.end = prev();
                cur = [];
                branch = new AST_Default({
                    start : (tmp = S.token, next(), expect(":"), tmp),
                    body  : cur
                });
                a.push(branch);
            } else {
                if (!cur) unexpected();
                cur.push(statement());
            }
        }
        if (branch) branch.end = prev();
        next();
        return a;
    }

    function try_() {
        var body = block_(), bcatch = null, bfinally = null;
        if (is("keyword", "catch")) {
            var start = S.token;
            next();
            if (is("punc", "{")) {
                var name = null;
            } else {
                expect("(");
                var name = parameter(undefined, AST_SymbolCatch);
                expect(")");
            }
            bcatch = new AST_Catch({
                start   : start,
                argname : name,
                body    : block_(),
                end     : prev()
            });
        }
        if (is("keyword", "finally")) {
            var start = S.token;
            next();
            bfinally = new AST_Finally({
                start : start,
                body  : block_(),
                end   : prev()
            });
        }
        if (!bcatch && !bfinally)
            croak("Missing catch/finally blocks");
        return new AST_Try({
            body     : body,
            bcatch   : bcatch,
            bfinally : bfinally
        });
    }

    function vardefs(no_in, kind) {
        var a = [];
        var def;
        for (;;) {
            var sym_type =
                kind === "var" ? AST_SymbolVar :
                kind === "const" ? AST_SymbolConst :
                kind === "let" ? AST_SymbolLet : null;
            if (is("punc", "{") || is("punc", "[")) {
                def = new AST_VarDef({
                    start: S.token,
                    name: binding_element(undefined ,sym_type),
                    value: is("operator", "=") ? (expect_token("operator", "="), expression(false, no_in)) : null,
                    end: prev()
                });
            } else {
                def = new AST_VarDef({
                    start : S.token,
                    name  : as_symbol(sym_type),
                    value : is("operator", "=")
                        ? (next(), expression(false, no_in))
                        : !no_in && kind === "const"
                            ? croak("Missing initializer in const declaration") : null,
                    end   : prev()
                });
                if (def.name.name == "import") croak("Unexpected token: import");
            }
            a.push(def);
            if (!is("punc", ","))
                break;
            next();
        }
        return a;
    }

    var var_ = function(no_in) {
        return new AST_Var({
            start       : prev(),
            definitions : vardefs(no_in, "var"),
            end         : prev()
        });
    };

    var let_ = function(no_in) {
        return new AST_Let({
            start       : prev(),
            definitions : vardefs(no_in, "let"),
            end         : prev()
        });
    };

    var const_ = function(no_in) {
        return new AST_Const({
            start       : prev(),
            definitions : vardefs(no_in, "const"),
            end         : prev()
        });
    };

    var new_ = function(allow_calls) {
        var start = S.token;
        expect_token("operator", "new");
        if (is("punc", ".")) {
            next();
            expect_token("name", "target");
            return subscripts(new AST_NewTarget({
                start : start,
                end   : prev()
            }), allow_calls);
        }
        var newexp = expr_atom(false), args;
        if (is("punc", "(")) {
            next();
            args = expr_list(")", options.ecma >= 8);
        } else {
            args = [];
        }
        var call = new AST_New({
            start      : start,
            expression : newexp,
            args       : args,
            end        : prev()
        });
        mark_pure(call);
        return subscripts(call, allow_calls);
    };

    function as_atom_node() {
        var tok = S.token, ret;
        switch (tok.type) {
          case "name":
            ret = _make_symbol(AST_SymbolRef);
            break;
          case "num":
            ret = new AST_Number({ start: tok, end: tok, value: tok.value });
            break;
          case "string":
            ret = new AST_String({
                start : tok,
                end   : tok,
                value : tok.value,
                quote : tok.quote
            });
            break;
          case "regexp":
            ret = new AST_RegExp({ start: tok, end: tok, value: tok.value });
            break;
          case "atom":
            switch (tok.value) {
              case "false":
                ret = new AST_False({ start: tok, end: tok });
                break;
              case "true":
                ret = new AST_True({ start: tok, end: tok });
                break;
              case "null":
                ret = new AST_Null({ start: tok, end: tok });
                break;
            }
            break;
        }
        next();
        return ret;
    }

    function to_fun_args(ex, _, __, default_seen_above) {
        var insert_default = function(ex, default_value) {
            if (default_value) {
                return new AST_DefaultAssign({
                    start: ex.start,
                    left: ex,
                    operator: "=",
                    right: default_value,
                    end: default_value.end
                });
            }
            return ex;
        };
        if (ex instanceof AST_Object) {
            return insert_default(new AST_Destructuring({
                start: ex.start,
                end: ex.end,
                is_array: false,
                names: ex.properties.map(to_fun_args)
            }), default_seen_above);
        } else if (ex instanceof AST_ObjectKeyVal) {
            ex.value = to_fun_args(ex.value, 0, [ex.key]);
            return insert_default(ex, default_seen_above);
        } else if (ex instanceof AST_Hole) {
            return ex;
        } else if (ex instanceof AST_Destructuring) {
            ex.names = ex.names.map(to_fun_args);
            return insert_default(ex, default_seen_above);
        } else if (ex instanceof AST_SymbolRef) {
            return insert_default(new AST_SymbolFunarg({
                name: ex.name,
                start: ex.start,
                end: ex.end
            }), default_seen_above);
        } else if (ex instanceof AST_Expansion) {
            ex.expression = to_fun_args(ex.expression);
            return insert_default(ex, default_seen_above);
        } else if (ex instanceof AST_Array) {
            return insert_default(new AST_Destructuring({
                start: ex.start,
                end: ex.end,
                is_array: true,
                names: ex.elements.map(to_fun_args)
            }), default_seen_above);
        } else if (ex instanceof AST_Assign) {
            return insert_default(to_fun_args(ex.left, undefined, undefined, ex.right), default_seen_above);
        } else if (ex instanceof AST_DefaultAssign) {
            ex.left = to_fun_args(ex.left, 0, [ex.left]);
            return ex;
        } else {
            croak("Invalid function parameter", ex.start.line, ex.start.col);
        }
    }

    var expr_atom = function(allow_calls, allow_arrows) {
        if (is("operator", "new")) {
            return new_(allow_calls);
        }
        var start = S.token;
        var peeked;
        var async = is("name", "async")
            && (peeked = peek()).value != "["
            && peeked.type != "arrow"
            && as_atom_node();
        if (is("punc")) {
            switch (S.token.value) {
              case "(":
                if (async && !allow_calls) break;
                var exprs = params_or_seq_(allow_arrows, !async);
                if (allow_arrows && is("arrow", "=>")) {
                    return arrow_function(start, exprs.map(to_fun_args), !!async);
                }
                var ex = async ? new AST_Call({
                    expression: async,
                    args: exprs
                }) : exprs.length == 1 ? exprs[0] : new AST_Sequence({
                    expressions: exprs
                });
                if (ex.start) {
                    var len = start.comments_before.length;
                    [].unshift.apply(ex.start.comments_before, start.comments_before);
                    start.comments_before = ex.start.comments_before;
                    start.comments_before_length = len;
                    if (len == 0 && start.comments_before.length > 0) {
                        var comment = start.comments_before[0];
                        if (!comment.nlb) {
                            comment.nlb = start.nlb;
                            start.nlb = false;
                        }
                    }
                    start.comments_after = ex.start.comments_after;
                }
                ex.start = start;
                var end = prev();
                if (ex.end) {
                    end.comments_before = ex.end.comments_before;
                    [].push.apply(ex.end.comments_after, end.comments_after);
                    end.comments_after = ex.end.comments_after;
                }
                ex.end = end;
                if (ex instanceof AST_Call) mark_pure(ex);
                return subscripts(ex, allow_calls);
              case "[":
                return subscripts(array_(), allow_calls);
              case "{":
                return subscripts(object_or_destructuring_(), allow_calls);
            }
            if (!async) unexpected();
        }
        if (allow_arrows && is("name") && is_token(peek(), "arrow")) {
            var param = new AST_SymbolFunarg({
                name: S.token.value,
                start: start,
                end: start,
            });
            next();
            return arrow_function(start, [param], !!async);
        }
        if (is("keyword", "function")) {
            next();
            var func = function_(AST_Function, false, !!async);
            func.start = start;
            func.end = prev();
            return subscripts(func, allow_calls);
        }
        if (async) return subscripts(async, allow_calls);
        if (is("keyword", "class")) {
            next();
            var cls = class_(AST_ClassExpression);
            cls.start = start;
            cls.end = prev();
            return subscripts(cls, allow_calls);
        }
        if (is("template_head")) {
            return subscripts(template_string(false), allow_calls);
        }
        if (ATOMIC_START_TOKEN(S.token.type)) {
            return subscripts(as_atom_node(), allow_calls);
        }
        unexpected();
    };

    function template_string(tagged) {
        var segments = [], start = S.token;

        segments.push(new AST_TemplateSegment({
            start: S.token,
            raw: S.token.raw,
            value: S.token.value,
            end: S.token
        }));
        while (S.token.end === false) {
            next();
            handle_regexp();
            segments.push(expression(true));

            if (!is_token("template_substitution")) {
                unexpected();
            }

            segments.push(new AST_TemplateSegment({
                start: S.token,
                raw: S.token.raw,
                value: S.token.value,
                end: S.token
            }));
        }
        next();

        return new AST_TemplateString({
            start: start,
            segments: segments,
            end: S.token
        });
    }

    function expr_list(closing, allow_trailing_comma, allow_empty) {
        var first = true, a = [];
        while (!is("punc", closing)) {
            if (first) first = false; else expect(",");
            if (allow_trailing_comma && is("punc", closing)) break;
            if (is("punc", ",") && allow_empty) {
                a.push(new AST_Hole({ start: S.token, end: S.token }));
            } else if (is("expand", "...")) {
                next();
                a.push(new AST_Expansion({start: prev(), expression: expression(),end: S.token}));
            } else {
                a.push(expression(false));
            }
        }
        next();
        return a;
    }

    var array_ = embed_tokens(function() {
        expect("[");
        return new AST_Array({
            elements: expr_list("]", !options.strict, true)
        });
    });

    var create_accessor = embed_tokens(function(is_generator, is_async) {
        return function_(AST_Accessor, is_generator, is_async);
    });

    var object_or_destructuring_ = embed_tokens(function object_or_destructuring_() {
        var start = S.token, first = true, a = [];
        expect("{");
        while (!is("punc", "}")) {
            if (first) first = false; else expect(",");
            if (!options.strict && is("punc", "}"))
                // allow trailing comma
                break;

            start = S.token;
            if (start.type == "expand") {
                next();
                a.push(new AST_Expansion({
                    start: start,
                    expression: expression(false),
                    end: prev(),
                }));
                continue;
            }

            var name = as_property_name();
            var value;

            // Check property and fetch value
            if (!is("punc", ":")) {
                var concise = concise_method_or_getset(name, start);
                if (concise) {
                    a.push(concise);
                    continue;
                }

                value = new AST_SymbolRef({
                    start: prev(),
                    name: name,
                    end: prev()
                });
            } else if (name === null) {
                unexpected(prev());
            } else {
                next(); // `:` - see first condition
                value = expression(false);
            }

            // Check for default value and alter value accordingly if necessary
            if (is("operator", "=")) {
                next();
                value = new AST_Assign({
                    start: start,
                    left: value,
                    operator: "=",
                    right: expression(false),
                    end: prev()
                });
            }

            // Create property
            a.push(new AST_ObjectKeyVal({
                start: start,
                quote: start.quote,
                key: name instanceof AST_Node ? name : "" + name,
                value: value,
                end: prev()
            }));
        }
        next();
        return new AST_Object({ properties: a });
    });

    function class_(KindOfClass) {
        var start, method, class_name, extends_, a = [];

        S.input.push_directives_stack(); // Push directive stack, but not scope stack
        S.input.add_directive("use strict");

        if (S.token.type == "name" && S.token.value != "extends") {
            class_name = as_symbol(KindOfClass === AST_DefClass ? AST_SymbolDefClass : AST_SymbolClass);
        }

        if (KindOfClass === AST_DefClass && !class_name) {
            unexpected();
        }

        if (S.token.value == "extends") {
            next();
            extends_ = expression(true);
        }

        expect("{");

        if (is("punc", ";")) { next(); }  // Leading semicolons are okay in class bodies.
        while (!is("punc", "}")) {
            start = S.token;
            method = concise_method_or_getset(as_property_name(), start, true);
            if (!method) { unexpected(); }
            a.push(method);
            if (is("punc", ";")) { next(); }
        }

        S.input.pop_directives_stack();

        next();

        return new KindOfClass({
            start: start,
            name: class_name,
            extends: extends_,
            properties: a,
            end: prev(),
        });
    }

    function concise_method_or_getset(name, start, is_class) {
        var get_ast = function(name, token) {
            if (typeof name === "string" || typeof name === "number") {
                return new AST_SymbolMethod({
                    start: token,
                    name: "" + name,
                    end: prev()
                });
            } else if (name === null) {
                unexpected();
            }
            return name;
        };
        var is_async = false;
        var is_static = false;
        var is_generator = false;
        var property_token = start;
        if (is_class && name === "static" && !is("punc", "(")) {
            is_static = true;
            property_token = S.token;
            name = as_property_name();
        }
        if (name === "async" && !is("punc", "(") && !is("punc", ",") && !is("punc", "}")) {
            is_async = true;
            property_token = S.token;
            name = as_property_name();
        }
        if (name === null) {
            is_generator = true;
            property_token = S.token;
            name = as_property_name();
            if (name === null) {
                unexpected();
            }
        }
        if (is("punc", "(")) {
            name = get_ast(name, start);
            var node = new AST_ConciseMethod({
                start       : start,
                static      : is_static,
                is_generator: is_generator,
                async       : is_async,
                key         : name,
                quote       : name instanceof AST_SymbolMethod ?
                              property_token.quote : undefined,
                value       : create_accessor(is_generator, is_async),
                end         : prev()
            });
            return node;
        }
        property_token = S.token;
        if (name == "get") {
            if (!is("punc") || is("punc", "[")) {
                name = get_ast(as_property_name(), start);
                return new AST_ObjectGetter({
                    start : start,
                    static: is_static,
                    key   : name,
                    quote : name instanceof AST_SymbolMethod ?
                            property_token.quote : undefined,
                    value : create_accessor(),
                    end   : prev()
                });
            }
        } else if (name == "set") {
            if (!is("punc") || is("punc", "[")) {
                name = get_ast(as_property_name(), start);
                return new AST_ObjectSetter({
                    start : start,
                    static: is_static,
                    key   : name,
                    quote : name instanceof AST_SymbolMethod ?
                            property_token.quote : undefined,
                    value : create_accessor(),
                    end   : prev()
                });
            }
        }
    }

    function import_() {
        var start = prev();
        var imported_name;
        var imported_names;
        if (is("name")) {
            imported_name = as_symbol(AST_SymbolImport);
        }

        if (is("punc", ",")) {
            next();
        }

        imported_names = map_names(true);

        if (imported_names || imported_name) {
            expect_token("name", "from");
        }
        var mod_str = S.token;
        if (mod_str.type !== "string") {
            unexpected();
        }
        next();
        return new AST_Import({
            start: start,
            imported_name: imported_name,
            imported_names: imported_names,
            module_name: new AST_String({
                start: mod_str,
                value: mod_str.value,
                quote: mod_str.quote,
                end: mod_str,
            }),
            end: S.token,
        });
    }

    function map_name(is_import) {
        function make_symbol(type) {
            return new type({
                name: as_property_name(),
                start: prev(),
                end: prev()
            });
        }

        var foreign_type = is_import ? AST_SymbolImportForeign : AST_SymbolExportForeign;
        var type = is_import ? AST_SymbolImport : AST_SymbolExport;
        var start = S.token;
        var foreign_name;
        var name;

        if (is_import) {
            foreign_name = make_symbol(foreign_type);
        } else {
            name = make_symbol(type);
        }
        if (is("name", "as")) {
            next();  // The "as" word
            if (is_import) {
                name = make_symbol(type);
            } else {
                foreign_name = make_symbol(foreign_type);
            }
        } else if (is_import) {
            name = new type(foreign_name);
        } else {
            foreign_name = new foreign_type(name);
        }

        return new AST_NameMapping({
            start: start,
            foreign_name: foreign_name,
            name: name,
            end: prev(),
        });
    }

    function map_nameAsterisk(is_import, name) {
        var foreign_type = is_import ? AST_SymbolImportForeign : AST_SymbolExportForeign;
        var type = is_import ? AST_SymbolImport : AST_SymbolExport;
        var start = S.token;
        var foreign_name;
        var end = prev();

        name = name || new type({
            name: "*",
            start: start,
            end: end,
        });

        foreign_name = new foreign_type({
            name: "*",
            start: start,
            end: end,
        });

        return new AST_NameMapping({
            start: start,
            foreign_name: foreign_name,
            name: name,
            end: end,
        });
    }

    function map_names(is_import) {
        var names;
        if (is("punc", "{")) {
            next();
            names = [];
            while (!is("punc", "}")) {
                names.push(map_name(is_import));
                if (is("punc", ",")) {
                    next();
                }
            }
            next();
        } else if (is("operator", "*")) {
            var name;
            next();
            if (is_import && is("name", "as")) {
                next();  // The "as" word
                name = as_symbol(is_import ? AST_SymbolImport : AST_SymbolExportForeign);
            }
            names = [map_nameAsterisk(is_import, name)];
        }
        return names;
    }

    function export_() {
        var start = S.token;
        var is_default;
        var exported_names;

        if (is("keyword", "default")) {
            is_default = true;
            next();
        } else if (exported_names = map_names(false)) {
            if (is("name", "from")) {
                next();

                var mod_str = S.token;
                if (mod_str.type !== "string") {
                    unexpected();
                }
                next();

                return new AST_Export({
                    start: start,
                    is_default: is_default,
                    exported_names: exported_names,
                    module_name: new AST_String({
                        start: mod_str,
                        value: mod_str.value,
                        quote: mod_str.quote,
                        end: mod_str,
                    }),
                    end: prev(),
                });
            } else {
                return new AST_Export({
                    start: start,
                    is_default: is_default,
                    exported_names: exported_names,
                    end: prev(),
                });
            }
        }

        var node;
        var exported_value;
        var exported_definition;
        if (is("punc", "{")
            || is_default
                && (is("keyword", "class") || is("keyword", "function"))
                && is_token(peek(), "punc")) {
            exported_value = expression(false);
            semicolon();
        } else if ((node = statement(is_default)) instanceof AST_Definitions && is_default) {
            unexpected(node.start);
        } else if (node instanceof AST_Definitions || node instanceof AST_Lambda || node instanceof AST_DefClass) {
            exported_definition = node;
        } else if (node instanceof AST_SimpleStatement) {
            exported_value = node.body;
        } else {
            unexpected(node.start);
        }

        return new AST_Export({
            start: start,
            is_default: is_default,
            exported_value: exported_value,
            exported_definition: exported_definition,
            end: prev(),
        });
    }

    function as_property_name() {
        var tmp = S.token;
        switch (tmp.type) {
          case "punc":
            if (tmp.value === "[") {
                next();
                var ex = expression(false);
                expect("]");
                return ex;
            } else unexpected(tmp);
          case "operator":
            if (tmp.value === "*") {
                next();
                return null;
            }
            if (["delete", "in", "instanceof", "new", "typeof", "void"].indexOf(tmp.value) === -1) {
                unexpected(tmp);
            }
          case "name":
            if (tmp.value == "yield") {
                if (is_in_generator()) {
                    token_error(tmp, "Yield cannot be used as identifier inside generators");
                } else if (!is_token(peek(), "punc", ":")
                    && !is_token(peek(), "punc", "(")
                    && S.input.has_directive("use strict")) {
                    token_error(tmp, "Unexpected yield identifier inside strict mode");
                }
            }
          case "string":
          case "num":
          case "keyword":
          case "atom":
            next();
            return tmp.value;
          default:
            unexpected(tmp);
        }
    }

    function as_name() {
        var tmp = S.token;
        if (tmp.type != "name") unexpected();
        next();
        return tmp.value;
    }

    function _make_symbol(type) {
        var name = S.token.value;
        return new (name == "this" ? AST_This :
                    name == "super" ? AST_Super :
                    type)({
            name  : String(name),
            start : S.token,
            end   : S.token
        });
    }

    function _verify_symbol(sym) {
        var name = sym.name;
        if (is_in_generator() && name == "yield") {
            token_error(sym.start, "Yield cannot be used as identifier inside generators");
        }
        if (S.input.has_directive("use strict")) {
            if (name == "yield") {
                token_error(sym.start, "Unexpected yield identifier inside strict mode");
            }
            if (sym instanceof AST_SymbolDeclaration && (name == "arguments" || name == "eval")) {
                token_error(sym.start, "Unexpected " + name + " in strict mode");
            }
        }
    }

    function as_symbol(type, noerror) {
        if (!is("name")) {
            if (!noerror) croak("Name expected");
            return null;
        }
        var sym = _make_symbol(type);
        _verify_symbol(sym);
        next();
        return sym;
    }

    function mark_pure(call) {
        var start = call.start;
        var comments = start.comments_before;
        var i = HOP(start, "comments_before_length") ? start.comments_before_length : comments.length;
        while (--i >= 0) {
            var comment = comments[i];
            if (/[@#]__PURE__/.test(comment.value)) {
                call.pure = comment;
                break;
            }
        }
    }

    var subscripts = function(expr, allow_calls) {
        var start = expr.start;
        if (is("punc", ".")) {
            next();
            return subscripts(new AST_Dot({
                start      : start,
                expression : expr,
                property   : as_name(),
                end        : prev()
            }), allow_calls);
        }
        if (is("punc", "[")) {
            next();
            var prop = expression(true);
            expect("]");
            return subscripts(new AST_Sub({
                start      : start,
                expression : expr,
                property   : prop,
                end        : prev()
            }), allow_calls);
        }
        if (allow_calls && is("punc", "(")) {
            next();
            var call = new AST_Call({
                start      : start,
                expression : expr,
                args       : call_args(),
                end        : prev()
            });
            mark_pure(call);
            return subscripts(call, true);
        }
        if (is("template_head")) {
            return subscripts(new AST_PrefixedTemplateString({
                start: start,
                prefix: expr,
                template_string: template_string(true),
                end: prev()
            }), allow_calls);
        }
        return expr;
    };

    var call_args = embed_tokens(function _call_args() {
        var args = [];
        while (!is("punc", ")")) {
            if (is("expand", "...")) {
                next();
                args.push(new AST_Expansion({
                    start: prev(),
                    expression: expression(false),
                    end: prev()
                }));
            } else {
                args.push(expression(false));
            }
            if (!is("punc", ")")) {
                expect(",");
                if (is("punc", ")") && options.ecma < 8) unexpected();
            }
        }
        next();
        return args;
    });

    var maybe_unary = function(allow_calls, allow_arrows) {
        var start = S.token;
        if (start.type == "name" && start.value == "await") {
            if (is_in_async()) {
                next();
                return _await_expression();
            } else if (S.input.has_directive("use strict")) {
                token_error(S.token, "Unexpected await identifier inside strict mode");
            }
        }
        if (is("operator") && UNARY_PREFIX(start.value)) {
            next();
            handle_regexp();
            var ex = make_unary(AST_UnaryPrefix, start, maybe_unary(allow_calls));
            ex.start = start;
            ex.end = prev();
            return ex;
        }
        var val = expr_atom(allow_calls, allow_arrows);
        while (is("operator") && UNARY_POSTFIX(S.token.value) && !has_newline_before(S.token)) {
            if (val instanceof AST_Arrow) unexpected();
            val = make_unary(AST_UnaryPostfix, S.token, val);
            val.start = start;
            val.end = S.token;
            next();
        }
        return val;
    };

    function make_unary(ctor, token, expr) {
        var op = token.value;
        switch (op) {
          case "++":
          case "--":
            if (!is_assignable(expr))
                croak("Invalid use of " + op + " operator", token.line, token.col, token.pos);
            break;
          case "delete":
            if (expr instanceof AST_SymbolRef && S.input.has_directive("use strict"))
                croak("Calling delete on expression not allowed in strict mode", expr.start.line, expr.start.col, expr.start.pos);
            break;
        }
        return new ctor({ operator: op, expression: expr });
    }

    var expr_op = function(left, min_prec, no_in) {
        var op = is("operator") ? S.token.value : null;
        if (op == "in" && no_in) op = null;
        if (op == "**" && left instanceof AST_UnaryPrefix
            /* unary token in front not allowed - parenthesis required */
            && !is_token(left.start, "punc", "(")
            && left.operator !== "--" && left.operator !== "++")
                unexpected(left.start);
        var prec = op != null ? PRECEDENCE[op] : null;
        if (prec != null && (prec > min_prec || (op === "**" && min_prec === prec))) {
            next();
            var right = expr_op(maybe_unary(true), prec, no_in);
            return expr_op(new AST_Binary({
                start    : left.start,
                left     : left,
                operator : op,
                right    : right,
                end      : right.end
            }), min_prec, no_in);
        }
        return left;
    };

    function expr_ops(no_in) {
        return expr_op(maybe_unary(true, true), 0, no_in);
    }

    var maybe_conditional = function(no_in) {
        var start = S.token;
        var expr = expr_ops(no_in);
        if (is("operator", "?")) {
            next();
            var yes = expression(false);
            expect(":");
            return new AST_Conditional({
                start       : start,
                condition   : expr,
                consequent  : yes,
                alternative : expression(false, no_in),
                end         : prev()
            });
        }
        return expr;
    };

    function is_assignable(expr) {
        return expr instanceof AST_PropAccess || expr instanceof AST_SymbolRef;
    }

    function to_destructuring(node) {
        if (node instanceof AST_Object) {
            node = new AST_Destructuring({
                start: node.start,
                names: node.properties.map(to_destructuring),
                is_array: false,
                end: node.end
            });
        } else if (node instanceof AST_Array) {
            var names = [];

            for (var i = 0; i < node.elements.length; i++) {
                // Only allow expansion as last element
                if (node.elements[i] instanceof AST_Expansion) {
                    if (i + 1 !== node.elements.length) {
                        token_error(node.elements[i].start, "Spread must the be last element in destructuring array");
                    }
                    node.elements[i].expression = to_destructuring(node.elements[i].expression);
                }

                names.push(to_destructuring(node.elements[i]));
            }

            node = new AST_Destructuring({
                start: node.start,
                names: names,
                is_array: true,
                end: node.end
            });
        } else if (node instanceof AST_ObjectProperty) {
            node.value = to_destructuring(node.value);
        } else if (node instanceof AST_Assign) {
            node = new AST_DefaultAssign({
                start: node.start,
                left: node.left,
                operator: "=",
                right: node.right,
                end: node.end
            });
        }
        return node;
    }

    // In ES6, AssignmentExpression can also be an ArrowFunction
    var maybe_assign = function(no_in) {
        handle_regexp();
        var start = S.token;

        if (start.type == "name" && start.value == "yield") {
            if (is_in_generator()) {
                next();
                return _yield_expression();
            } else if (S.input.has_directive("use strict")) {
                token_error(S.token, "Unexpected yield identifier inside strict mode");
            }
        }

        var left = maybe_conditional(no_in);
        var val = S.token.value;

        if (is("operator") && ASSIGNMENT(val)) {
            if (is_assignable(left) || (left = to_destructuring(left)) instanceof AST_Destructuring) {
                next();
                return new AST_Assign({
                    start    : start,
                    left     : left,
                    operator : val,
                    right    : maybe_assign(no_in),
                    end      : prev()
                });
            }
            croak("Invalid assignment");
        }
        return left;
    };

    var expression = function(commas, no_in) {
        var start = S.token;
        var exprs = [];
        while (true) {
            exprs.push(maybe_assign(no_in));
            if (!commas || !is("punc", ",")) break;
            next();
            commas = true;
        }
        return exprs.length == 1 ? exprs[0] : new AST_Sequence({
            start       : start,
            expressions : exprs,
            end         : peek()
        });
    };

    function in_loop(cont) {
        ++S.in_loop;
        var ret = cont();
        --S.in_loop;
        return ret;
    }

    if (options.expression) {
        return expression(true);
    }

    return (function() {
        var start = S.token;
        var body = [];
        S.input.push_directives_stack();
        if (options.module) S.input.add_directive("use strict");
        while (!is("eof"))
            body.push(statement());
        S.input.pop_directives_stack();
        var end = prev();
        var toplevel = options.toplevel;
        if (toplevel) {
            toplevel.body = toplevel.body.concat(body);
            toplevel.end = end;
        } else {
            toplevel = new AST_Toplevel({ start: start, body: body, end: end });
        }
        return toplevel;
    })();

}
/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

function SymbolDef(scope, orig, init) {
    this.name = orig.name;
    this.orig = [ orig ];
    this.init = init;
    this.eliminated = 0;
    this.scope = scope;
    this.references = [];
    this.replaced = 0;
    this.global = false;
    this.export = false;
    this.mangled_name = null;
    this.undeclared = false;
    this.id = SymbolDef.next_id++;
}

SymbolDef.next_id = 1;

var MASK_EXPORT_DONT_MANGLE = 1 << 0;
var MASK_EXPORT_WANT_MANGLE = 1 << 1;

SymbolDef.prototype = {
    unmangleable: function(options) {
        if (!options) options = {};

        return this.global && !options.toplevel
            || (this.export & MASK_EXPORT_DONT_MANGLE)
            || this.undeclared
            || !options.eval && this.scope.pinned()
            || (this.orig[0] instanceof AST_SymbolLambda
                  || this.orig[0] instanceof AST_SymbolDefun) && keep_name(options.keep_fnames, this.orig[0].name)
            || this.orig[0] instanceof AST_SymbolMethod
            || (this.orig[0] instanceof AST_SymbolClass
                  || this.orig[0] instanceof AST_SymbolDefClass) && keep_name(options.keep_classnames, this.orig[0].name);
    },
    mangle: function(options) {
        var cache = options.cache && options.cache.props;
        if (this.global && cache && cache.has(this.name)) {
            this.mangled_name = cache.get(this.name);
        } else if (!this.mangled_name && !this.unmangleable(options)) {
            var s = this.scope;
            var sym = this.orig[0];
            if (options.ie8 && sym instanceof AST_SymbolLambda)
                s = s.parent_scope;
            var def;
            if (def = this.redefined()) {
                this.mangled_name = def.mangled_name || def.name;
            } else
                this.mangled_name = s.next_mangled(options, this);
            if (this.global && cache) {
                cache.set(this.name, this.mangled_name);
            }
        }
    },
    redefined: function() {
        return this.defun && this.defun.variables.get(this.name);
    }
};

AST_Toplevel.DEFMETHOD("figure_out_scope", function(options) {
    options = defaults(options, {
        cache: null,
        ie8: false,
        safari10: false,
    });

    // pass 1: setup scope chaining and handle definitions
    var self = this;
    var scope = self.parent_scope = null;
    var labels = new Dictionary();
    var defun = null;
    var in_destructuring = null;
    var for_scopes = [];
    var tw = new TreeWalker(function(node, descend) {
        if (node.is_block_scope()) {
            var save_scope = scope;
            node.block_scope = scope = new AST_Scope(node);
            scope.init_scope_vars(save_scope);
            if (!(node instanceof AST_Scope)) {
                scope.uses_with = save_scope.uses_with;
                scope.uses_eval = save_scope.uses_eval;
                scope.directives = save_scope.directives;
            }
            if (options.safari10) {
                if (node instanceof AST_For || node instanceof AST_ForIn) {
                    for_scopes.push(scope);
                }
            }
            descend();
            scope = save_scope;
            return true;
        }
        if (node instanceof AST_Destructuring) {
            in_destructuring = node;  // These don't nest
            descend();
            in_destructuring = null;
            return true;
        }
        if (node instanceof AST_Scope) {
            node.init_scope_vars(scope);
            var save_scope = scope;
            var save_defun = defun;
            var save_labels = labels;
            defun = scope = node;
            labels = new Dictionary();
            descend();
            scope = save_scope;
            defun = save_defun;
            labels = save_labels;
            return true;        // don't descend again in TreeWalker
        }
        if (node instanceof AST_LabeledStatement) {
            var l = node.label;
            if (labels.has(l.name)) {
                throw new Error(string_template("Label {name} defined twice", l));
            }
            labels.set(l.name, l);
            descend();
            labels.del(l.name);
            return true;        // no descend again
        }
        if (node instanceof AST_With) {
            for (var s = scope; s; s = s.parent_scope)
                s.uses_with = true;
            return;
        }
        if (node instanceof AST_Symbol) {
            node.scope = scope;
        }
        if (node instanceof AST_Label) {
            node.thedef = node;
            node.references = [];
        }
        if (node instanceof AST_SymbolLambda) {
            defun.def_function(node, node.name == "arguments" ? undefined : defun);
        } else if (node instanceof AST_SymbolDefun) {
            // Careful here, the scope where this should be defined is
            // the parent scope.  The reason is that we enter a new
            // scope when we encounter the AST_Defun node (which is
            // instanceof AST_Scope) but we get to the symbol a bit
            // later.
            mark_export((node.scope = defun.parent_scope.get_defun_scope()).def_function(node, defun), 1);
        } else if (node instanceof AST_SymbolClass) {
            mark_export(defun.def_variable(node, defun), 1);
        } else if (node instanceof AST_SymbolImport) {
            scope.def_variable(node);
        } else if (node instanceof AST_SymbolDefClass) {
            // This deals with the name of the class being available
            // inside the class.
            mark_export((node.scope = defun.parent_scope).def_function(node, defun), 1);
        } else if (node instanceof AST_SymbolVar
            || node instanceof AST_SymbolLet
            || node instanceof AST_SymbolConst) {
            var def;
            if (node instanceof AST_SymbolBlockDeclaration) {
                def = scope.def_variable(node, null);
            } else {
                def = defun.def_variable(node, node.TYPE == "SymbolVar" ? null : undefined);
            }
            if (!all(def.orig, function(sym) {
                if (sym === node) return true;
                if (node instanceof AST_SymbolBlockDeclaration) {
                    return sym instanceof AST_SymbolLambda;
                }
                return !(sym instanceof AST_SymbolLet || sym instanceof AST_SymbolConst);
            })) {
                js_error(
                    node.name + " redeclared",
                    node.start.file,
                    node.start.line,
                    node.start.col,
                    node.start.pos
                );
            }
            if (!(node instanceof AST_SymbolFunarg)) mark_export(def, 2);
            def.destructuring = in_destructuring;
            if (defun !== scope) {
                node.mark_enclosed(options);
                var def = scope.find_variable(node);
                if (node.thedef !== def) {
                    node.thedef = def;
                    node.reference(options);
                }
            }
        } else if (node instanceof AST_SymbolCatch) {
            scope.def_variable(node).defun = defun;
        } else if (node instanceof AST_LabelRef) {
            var sym = labels.get(node.name);
            if (!sym) throw new Error(string_template("Undefined label {name} [{line},{col}]", {
                name: node.name,
                line: node.start.line,
                col: node.start.col
            }));
            node.thedef = sym;
        }
        if (!(scope instanceof AST_Toplevel) && (node instanceof AST_Export || node instanceof AST_Import)) {
            js_error(
                node.TYPE + " statement may only appear at top level",
                node.start.file,
                node.start.line,
                node.start.col,
                node.start.pos
            );
        }

        function mark_export(def, level) {
            if (in_destructuring) {
                var i = 0;
                do {
                    level++;
                } while (tw.parent(i++) !== in_destructuring);
            }
            var node = tw.parent(level);
            if (def.export = node instanceof AST_Export && MASK_EXPORT_DONT_MANGLE) {
                var exported = node.exported_definition;
                if ((exported instanceof AST_Defun || exported instanceof AST_DefClass) && node.is_default) {
                    def.export = MASK_EXPORT_WANT_MANGLE;
                }
            }
        }
    });
    self.walk(tw);

    // pass 2: find back references and eval
    self.globals = new Dictionary();
    var tw = new TreeWalker(function(node, descend) {
        if (node instanceof AST_LoopControl && node.label) {
            node.label.thedef.references.push(node);
            return true;
        }
        if (node instanceof AST_SymbolRef) {
            var name = node.name;
            if (name == "eval" && tw.parent() instanceof AST_Call) {
                for (var s = node.scope; s && !s.uses_eval; s = s.parent_scope) {
                    s.uses_eval = true;
                }
            }
            var sym;
            if (tw.parent() instanceof AST_NameMapping && tw.parent(1).module_name
                || !(sym = node.scope.find_variable(name))) {
                sym = self.def_global(node);
                if (node instanceof AST_SymbolExport) sym.export = MASK_EXPORT_DONT_MANGLE;
            } else if (sym.scope instanceof AST_Lambda && name == "arguments") {
                sym.scope.uses_arguments = true;
            }
            node.thedef = sym;
            node.reference(options);
            if (node.scope.is_block_scope()
                && !(sym.orig[0] instanceof AST_SymbolBlockDeclaration)) {
                node.scope = node.scope.get_defun_scope();
            }
            return true;
        }
        // ensure mangling works if catch reuses a scope variable
        var def;
        if (node instanceof AST_SymbolCatch && (def = node.definition().redefined())) {
            var s = node.scope;
            while (s) {
                push_uniq(s.enclosed, def);
                if (s === def.scope) break;
                s = s.parent_scope;
            }
        }
    });
    self.walk(tw);

    // pass 3: work around IE8 and Safari catch scope bugs
    if (options.ie8 || options.safari10) {
        self.walk(new TreeWalker(function(node, descend) {
            if (node instanceof AST_SymbolCatch) {
                var name = node.name;
                var refs = node.thedef.references;
                var scope = node.thedef.defun;
                var def = scope.find_variable(name) || self.globals.get(name) || scope.def_variable(node);
                refs.forEach(function(ref) {
                    ref.thedef = def;
                    ref.reference(options);
                });
                node.thedef = def;
                node.reference(options);
                return true;
            }
        }));
    }

    // pass 4: add symbol definitions to loop scopes
    // Safari/Webkit bug workaround - loop init let variable shadowing argument.
    // https://github.com/mishoo/UglifyJS2/issues/1753
    // https://bugs.webkit.org/show_bug.cgi?id=171041
    if (options.safari10) {
        for (var i = 0; i < for_scopes.length; i++) {
            var scope = for_scopes[i];
            scope.parent_scope.variables.each(function(def) {
                push_uniq(scope.enclosed, def);
            });
        }
    }
});

AST_Toplevel.DEFMETHOD("def_global", function(node) {
    var globals = this.globals, name = node.name;
    if (globals.has(name)) {
        return globals.get(name);
    } else {
        var g = new SymbolDef(this, node);
        g.undeclared = true;
        g.global = true;
        globals.set(name, g);
        return g;
    }
});

AST_Scope.DEFMETHOD("init_scope_vars", function(parent_scope) {
    this.variables = new Dictionary();  // map name to AST_SymbolVar (variables defined in this scope; includes functions)
    this.functions = new Dictionary();  // map name to AST_SymbolDefun (functions defined in this scope)
    this.uses_with = false;             // will be set to true if this or some nested scope uses the `with` statement
    this.uses_eval = false;             // will be set to true if this or nested scope uses the global `eval`
    this.parent_scope = parent_scope;   // the parent scope
    this.enclosed = [];                 // a list of variables from this or outer scope(s) that are referenced from this or inner scopes
    this.cname = -1;                    // the current index for mangling functions/variables
});

AST_Node.DEFMETHOD("is_block_scope", return_false);
AST_Class.DEFMETHOD("is_block_scope", return_false);
AST_Lambda.DEFMETHOD("is_block_scope", return_false);
AST_Toplevel.DEFMETHOD("is_block_scope", return_false);
AST_SwitchBranch.DEFMETHOD("is_block_scope", return_false);
AST_Block.DEFMETHOD("is_block_scope", return_true);
AST_IterationStatement.DEFMETHOD("is_block_scope", return_true);

AST_Lambda.DEFMETHOD("init_scope_vars", function() {
    AST_Scope.prototype.init_scope_vars.apply(this, arguments);
    this.uses_arguments = false;
    this.def_variable(new AST_SymbolFunarg({
        name: "arguments",
        start: this.start,
        end: this.end
    }));
});

AST_Arrow.DEFMETHOD("init_scope_vars", function() {
    AST_Scope.prototype.init_scope_vars.apply(this, arguments);
    this.uses_arguments = false;
});

AST_Symbol.DEFMETHOD("mark_enclosed", function(options) {
    var def = this.definition();
    var s = this.scope;
    while (s) {
        push_uniq(s.enclosed, def);
        if (options.keep_fnames) {
            s.functions.each(function(d) {
                if (keep_name(options.keep_fnames, d.name)) {
                    push_uniq(def.scope.enclosed, d);
                }
            });
        }
        if (s === def.scope) break;
        s = s.parent_scope;
    }
});

AST_Symbol.DEFMETHOD("reference", function(options) {
    this.definition().references.push(this);
    this.mark_enclosed(options);
});

AST_Scope.DEFMETHOD("find_variable", function(name) {
    if (name instanceof AST_Symbol) name = name.name;
    return this.variables.get(name)
        || (this.parent_scope && this.parent_scope.find_variable(name));
});

AST_Scope.DEFMETHOD("def_function", function(symbol, init) {
    var def = this.def_variable(symbol, init);
    if (!def.init || def.init instanceof AST_Defun) def.init = init;
    this.functions.set(symbol.name, def);
    return def;
});

AST_Scope.DEFMETHOD("def_variable", function(symbol, init) {
    var def = this.variables.get(symbol.name);
    if (def) {
        def.orig.push(symbol);
        if (def.init && (def.scope !== symbol.scope || def.init instanceof AST_Function)) {
            def.init = init;
        }
    } else {
        def = new SymbolDef(this, symbol, init);
        this.variables.set(symbol.name, def);
        def.global = !this.parent_scope;
    }
    return symbol.thedef = def;
});

function next_mangled(scope, options) {
    var ext = scope.enclosed;
    out: while (true) {
        var m = base54(++scope.cname);
        if (!is_identifier(m)) continue; // skip over "do"

        // https://github.com/mishoo/UglifyJS2/issues/242 -- do not
        // shadow a name reserved from mangling.
        if (member(m, options.reserved)) continue;

        // we must ensure that the mangled name does not shadow a name
        // from some parent scope that is referenced in this or in
        // inner scopes.
        for (var i = ext.length; --i >= 0;) {
            var sym = ext[i];
            var name = sym.mangled_name || (sym.unmangleable(options) && sym.name);
            if (m == name) continue out;
        }
        return m;
    }
}

AST_Scope.DEFMETHOD("next_mangled", function(options) {
    return next_mangled(this, options);
});

AST_Toplevel.DEFMETHOD("next_mangled", function(options) {
    var name;
    do {
        name = next_mangled(this, options);
    } while (member(name, this.mangled_names));
    return name;
});

AST_Function.DEFMETHOD("next_mangled", function(options, def) {
    // #179, #326
    // in Safari strict mode, something like (function x(x){...}) is a syntax error;
    // a function expression's argument cannot shadow the function expression's name

    var tricky_def = def.orig[0] instanceof AST_SymbolFunarg && this.name && this.name.definition();

    // the function's mangled_name is null when keep_fnames is true
    var tricky_name = tricky_def ? tricky_def.mangled_name || tricky_def.name : null;

    while (true) {
        var name = next_mangled(this, options);
        if (!tricky_name || tricky_name != name)
            return name;
    }
});

AST_Symbol.DEFMETHOD("unmangleable", function(options) {
    var def = this.definition();
    return !def || def.unmangleable(options);
});

// labels are always mangleable
AST_Label.DEFMETHOD("unmangleable", return_false);

AST_Symbol.DEFMETHOD("unreferenced", function() {
    return !this.definition().references.length && !this.scope.pinned();
});

AST_Symbol.DEFMETHOD("definition", function() {
    return this.thedef;
});

AST_Symbol.DEFMETHOD("global", function() {
    return this.definition().global;
});

AST_Toplevel.DEFMETHOD("_default_mangler_options", function(options) {
    options = defaults(options, {
        eval        : false,
        ie8         : false,
        keep_classnames: false,
        keep_fnames : false,
        module      : false,
        reserved    : [],
        toplevel    : false,
    });
    if (options["module"]) {
        options.toplevel = true;
    }
    if (!Array.isArray(options.reserved)) options.reserved = [];
    // Never mangle arguments
    push_uniq(options.reserved, "arguments");
    return options;
});

AST_Toplevel.DEFMETHOD("mangle_names", function(options) {
    options = this._default_mangler_options(options);

    // We only need to mangle declaration nodes.  Special logic wired
    // into the code generator will display the mangled name if it's
    // present (and for AST_SymbolRef-s it'll use the mangled name of
    // the AST_SymbolDeclaration that it points to).
    var lname = -1;
    var to_mangle = [];

    var mangled_names = this.mangled_names = [];
    if (options.cache) {
        this.globals.each(collect);
        if (options.cache.props) {
            options.cache.props.each(function(mangled_name) {
                push_uniq(mangled_names, mangled_name);
            });
        }
    }

    var tw = new TreeWalker(function(node, descend) {
        if (node instanceof AST_LabeledStatement) {
            // lname is incremented when we get to the AST_Label
            var save_nesting = lname;
            descend();
            lname = save_nesting;
            return true;        // don't descend again in TreeWalker
        }
        if (node instanceof AST_Scope) {
            node.variables.each(collect);
            return;
        }
        if (node.is_block_scope()) {
            node.block_scope.variables.each(collect);
            return;
        }
        if (node instanceof AST_Label) {
            var name;
            do name = base54(++lname); while (!is_identifier(name));
            node.mangled_name = name;
            return true;
        }
        if (!(options.ie8 || options.safari10) && node instanceof AST_SymbolCatch) {
            to_mangle.push(node.definition());
            return;
        }
    });
    this.walk(tw);
    to_mangle.forEach(function(def) { def.mangle(options); });

    function collect(symbol) {
        if (!member(symbol.name, options.reserved)) {
            if (!(symbol.export & MASK_EXPORT_DONT_MANGLE)) {
                to_mangle.push(symbol);
            }
        }
    }
});

AST_Toplevel.DEFMETHOD("find_colliding_names", function(options) {
    var cache = options.cache && options.cache.props;
    var avoid = Object.create(null);
    options.reserved.forEach(to_avoid);
    this.globals.each(add_def);
    this.walk(new TreeWalker(function(node) {
        if (node instanceof AST_Scope) node.variables.each(add_def);
        if (node instanceof AST_SymbolCatch) add_def(node.definition());
    }));
    return avoid;

    function to_avoid(name) {
        avoid[name] = true;
    }

    function add_def(def) {
        var name = def.name;
        if (def.global && cache && cache.has(name)) name = cache.get(name);
        else if (!def.unmangleable(options)) return;
        to_avoid(name);
    }
});

AST_Toplevel.DEFMETHOD("expand_names", function(options) {
    base54.reset();
    base54.sort();
    options = this._default_mangler_options(options);
    var avoid = this.find_colliding_names(options);
    var cname = 0;
    this.globals.each(rename);
    this.walk(new TreeWalker(function(node) {
        if (node instanceof AST_Scope) node.variables.each(rename);
        if (node instanceof AST_SymbolCatch) rename(node.definition());
    }));

    function next_name() {
        var name;
        do {
            name = base54(cname++);
        } while (avoid[name] || !is_identifier(name));
        return name;
    }

    function rename(def) {
        if (def.global && options.cache) return;
        if (def.unmangleable(options)) return;
        if (member(def.name, options.reserved)) return;
        var d = def.redefined();
        def.name = d ? d.name : next_name();
        def.orig.forEach(function(sym) {
            sym.name = def.name;
        });
        def.references.forEach(function(sym) {
            sym.name = def.name;
        });
    }
});

AST_Node.DEFMETHOD("tail_node", return_this);
AST_Sequence.DEFMETHOD("tail_node", function() {
    return this.expressions[this.expressions.length - 1];
});

AST_Toplevel.DEFMETHOD("compute_char_frequency", function(options) {
    options = this._default_mangler_options(options);
    try {
        AST_Node.prototype.print = function(stream, force_parens) {
            this._print(stream, force_parens);
            if (this instanceof AST_Symbol && !this.unmangleable(options)) {
                base54.consider(this.name, -1);
            } else if (options.properties) {
                if (this instanceof AST_Dot) {
                    base54.consider(this.property, -1);
                } else if (this instanceof AST_Sub) {
                    skip_string(this.property);
                }
            }
        };
        base54.consider(this.print_to_string(), 1);
    } finally {
        AST_Node.prototype.print = AST_Node.prototype._print;
    }
    base54.sort();

    function skip_string(node) {
        if (node instanceof AST_String) {
            base54.consider(node.value, -1);
        } else if (node instanceof AST_Conditional) {
            skip_string(node.consequent);
            skip_string(node.alternative);
        } else if (node instanceof AST_Sequence) {
            skip_string(node.tail_node());
        }
    }
});

var base54 = (function() {
    var leading = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$_".split("");
    var digits = "0123456789".split("");
    var chars, frequency;
    function reset() {
        frequency = Object.create(null);
        leading.forEach(function(ch) {
            frequency[ch] = 0;
        });
        digits.forEach(function(ch) {
            frequency[ch] = 0;
        });
    }
    base54.consider = function(str, delta) {
        for (var i = str.length; --i >= 0;) {
            frequency[str[i]] += delta;
        }
    };
    function compare(a, b) {
        return frequency[b] - frequency[a];
    }
    base54.sort = function() {
        chars = mergeSort(leading, compare).concat(mergeSort(digits, compare));
    };
    base54.reset = reset;
    reset();
    function base54(num) {
        var ret = "", base = 54;
        num++;
        do {
            num--;
            ret += chars[num % base];
            num = Math.floor(num / base);
            base = 64;
        } while (num > 0);
        return ret;
    }
    return base54;
})();
/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

var EXPECT_DIRECTIVE = /^$|[;{][\s\n]*$/;

function is_some_comments(comment) {
    // multiline comment
    return comment.type == "comment2" && /@preserve|@license|@cc_on/i.test(comment.value);
}

function OutputStream(options) {

    var readonly = !options;
    options = defaults(options, {
        ascii_only       : false,
        beautify         : false,
        braces           : false,
        comments         : false,
        ecma             : 5,
        ie8              : false,
        indent_level     : 4,
        indent_start     : 0,
        inline_script    : true,
        keep_quoted_props: false,
        max_line_len     : false,
        preamble         : null,
        quote_keys       : false,
        quote_style      : 0,
        safari10         : false,
        semicolons       : true,
        shebang          : true,
        shorthand        : undefined,
        source_map       : null,
        webkit           : false,
        width            : 80,
        wrap_iife        : false,
    }, true);

    if (options.shorthand === undefined)
        options.shorthand = options.ecma > 5;

    // Convert comment option to RegExp if neccessary and set up comments filter
    var comment_filter = return_false; // Default case, throw all comments away
    if (options.comments) {
        var comments = options.comments;
        if (typeof options.comments === "string" && /^\/.*\/[a-zA-Z]*$/.test(options.comments)) {
            var regex_pos = options.comments.lastIndexOf("/");
            comments = new RegExp(
                options.comments.substr(1, regex_pos - 1),
                options.comments.substr(regex_pos + 1)
            );
        }
        if (comments instanceof RegExp) {
            comment_filter = function(comment) {
                return comment.type != "comment5" && comments.test(comment.value);
            };
        } else if (typeof comments === "function") {
            comment_filter = function(comment) {
                return comment.type != "comment5" && comments(this, comment);
            };
        } else if (comments === "some") {
            comment_filter = is_some_comments;
        } else { // NOTE includes "all" option
            comment_filter = return_true;
        }
    }

    var indentation = 0;
    var current_col = 0;
    var current_line = 1;
    var current_pos = 0;
    var OUTPUT = "";

    var to_utf8 = options.ascii_only ? function(str, identifier) {
        if (options.ecma >= 6) {
            str = str.replace(/[\ud800-\udbff][\udc00-\udfff]/g, function(ch) {
                var code = get_full_char_code(ch, 0).toString(16);
                return "\\u{" + code + "}";
            });
        }
        return str.replace(/[\u0000-\u001f\u007f-\uffff]/g, function(ch) {
            var code = ch.charCodeAt(0).toString(16);
            if (code.length <= 2 && !identifier) {
                while (code.length < 2) code = "0" + code;
                return "\\x" + code;
            } else {
                while (code.length < 4) code = "0" + code;
                return "\\u" + code;
            }
        });
    } : function(str) {
        var s = "";
        for (var i = 0, len = str.length; i < len; i++) {
            if (is_surrogate_pair_head(str[i]) && !is_surrogate_pair_tail(str[i + 1])
                || is_surrogate_pair_tail(str[i]) && !is_surrogate_pair_head(str[i - 1])) {
                s += "\\u" + str.charCodeAt(i).toString(16);
            } else {
                s += str[i];
            }
        }
        return s;
    };

    function make_string(str, quote) {
        var dq = 0, sq = 0;
        str = str.replace(/[\\\b\f\n\r\v\t\x22\x27\u2028\u2029\0\ufeff]/g,
          function(s, i) {
            switch (s) {
              case '"': ++dq; return '"';
              case "'": ++sq; return "'";
              case "\\": return "\\\\";
              case "\n": return "\\n";
              case "\r": return "\\r";
              case "\t": return "\\t";
              case "\b": return "\\b";
              case "\f": return "\\f";
              case "\x0B": return options.ie8 ? "\\x0B" : "\\v";
              case "\u2028": return "\\u2028";
              case "\u2029": return "\\u2029";
              case "\ufeff": return "\\ufeff";
              case "\0":
                  return /[0-9]/.test(get_full_char(str, i+1)) ? "\\x00" : "\\0";
            }
            return s;
        });
        function quote_single() {
            return "'" + str.replace(/\x27/g, "\\'") + "'";
        }
        function quote_double() {
            return '"' + str.replace(/\x22/g, '\\"') + '"';
        }
        function quote_template() {
            return "`" + str.replace(/`/g, "\\`") + "`";
        }
        str = to_utf8(str);
        if (quote === "`") return quote_template();
        switch (options.quote_style) {
          case 1:
            return quote_single();
          case 2:
            return quote_double();
          case 3:
            return quote == "'" ? quote_single() : quote_double();
          default:
            return dq > sq ? quote_single() : quote_double();
        }
    }

    function encode_string(str, quote) {
        var ret = make_string(str, quote);
        if (options.inline_script) {
            ret = ret.replace(/<\x2f(script)([>\/\t\n\f\r ])/gi, "<\\/$1$2");
            ret = ret.replace(/\x3c!--/g, "\\x3c!--");
            ret = ret.replace(/--\x3e/g, "--\\x3e");
        }
        return ret;
    }

    function make_name(name) {
        name = name.toString();
        name = to_utf8(name, true);
        return name;
    }

    function make_indent(back) {
        return repeat_string(" ", options.indent_start + indentation - back * options.indent_level);
    }

    /* -----[ beautification/minification ]----- */

    var has_parens = false;
    var might_need_space = false;
    var might_need_semicolon = false;
    var might_add_newline = 0;
    var need_newline_indented = false;
    var need_space = false;
    var newline_insert = -1;
    var last = "";
    var mapping_token, mapping_name, mappings = options.source_map && [];

    var do_add_mapping = mappings ? function() {
        mappings.forEach(function(mapping) {
            try {
                options.source_map.add(
                    mapping.token.file,
                    mapping.line, mapping.col,
                    mapping.token.line, mapping.token.col,
                    !mapping.name && mapping.token.type == "name" ? mapping.token.value : mapping.name
                );
            } catch(ex) {
                mapping.token.file != null && AST_Node.warn("Couldn't figure out mapping for {file}:{line},{col} → {cline},{ccol} [{name}]", {
                    file: mapping.token.file,
                    line: mapping.token.line,
                    col: mapping.token.col,
                    cline: mapping.line,
                    ccol: mapping.col,
                    name: mapping.name || ""
                });
            }
        });
        mappings = [];
    } : noop;

    var ensure_line_len = options.max_line_len ? function() {
        if (current_col > options.max_line_len) {
            if (might_add_newline) {
                var left = OUTPUT.slice(0, might_add_newline);
                var right = OUTPUT.slice(might_add_newline);
                if (mappings) {
                    var delta = right.length - current_col;
                    mappings.forEach(function(mapping) {
                        mapping.line++;
                        mapping.col += delta;
                    });
                }
                OUTPUT = left + "\n" + right;
                current_line++;
                current_pos++;
                current_col = right.length;
            }
            if (current_col > options.max_line_len) {
                AST_Node.warn("Output exceeds {max_line_len} characters", options);
            }
        }
        if (might_add_newline) {
            might_add_newline = 0;
            do_add_mapping();
        }
    } : noop;

    var requireSemicolonChars = makePredicate("( [ + * / - , . `");

    function print(str) {
        str = String(str);
        var ch = get_full_char(str, 0);
        var prev = get_full_char(last, last.length - 1);
        if (need_newline_indented && ch) {
            need_newline_indented = false;
            if (ch != "\n") {
                print("\n");
                indent();
            }
        }
        if (need_space && ch) {
            need_space = false;
            if (!/[\s;})]/.test(ch)) {
                space();
            }
        }
        newline_insert = -1;
        var prev = last.charAt(last.length - 1);
        if (might_need_semicolon) {
            might_need_semicolon = false;

            if (prev == ":" && ch == "}" || (!ch || ";}".indexOf(ch) < 0) && prev != ";") {
                if (options.semicolons || requireSemicolonChars(ch)) {
                    OUTPUT += ";";
                    current_col++;
                    current_pos++;
                } else {
                    ensure_line_len();
                    OUTPUT += "\n";
                    current_pos++;
                    current_line++;
                    current_col = 0;

                    if (/^\s+$/.test(str)) {
                        // reset the semicolon flag, since we didn't print one
                        // now and might still have to later
                        might_need_semicolon = true;
                    }
                }

                if (!options.beautify)
                    might_need_space = false;
            }
        }

        if (might_need_space) {
            if ((is_identifier_char(prev)
                    && (is_identifier_char(ch) || ch == "\\"))
                || (ch == "/" && ch == prev)
                || ((ch == "+" || ch == "-") && ch == last)
            ) {
                OUTPUT += " ";
                current_col++;
                current_pos++;
            }
            might_need_space = false;
        }

        if (mapping_token) {
            mappings.push({
                token: mapping_token,
                name: mapping_name,
                line: current_line,
                col: current_col
            });
            mapping_token = false;
            if (!might_add_newline) do_add_mapping();
        }

        OUTPUT += str;
        has_parens = str[str.length - 1] == "(";
        current_pos += str.length;
        var a = str.split(/\r?\n/), n = a.length - 1;
        current_line += n;
        current_col += a[0].length;
        if (n > 0) {
            ensure_line_len();
            current_col = a[n].length;
        }
        last = str;
    }

    var star = function() {
        print("*");
    };

    var space = options.beautify ? function() {
        print(" ");
    } : function() {
        might_need_space = true;
    };

    var indent = options.beautify ? function(half) {
        if (options.beautify) {
            print(make_indent(half ? 0.5 : 0));
        }
    } : noop;

    var with_indent = options.beautify ? function(col, cont) {
        if (col === true) col = next_indent();
        var save_indentation = indentation;
        indentation = col;
        var ret = cont();
        indentation = save_indentation;
        return ret;
    } : function(col, cont) { return cont(); };

    var newline = options.beautify ? function() {
        if (newline_insert < 0) return print("\n");
        if (OUTPUT[newline_insert] != "\n") {
            OUTPUT = OUTPUT.slice(0, newline_insert) + "\n" + OUTPUT.slice(newline_insert);
            current_pos++;
            current_line++;
        }
        newline_insert++;
    } : options.max_line_len ? function() {
        ensure_line_len();
        might_add_newline = OUTPUT.length;
    } : noop;

    var semicolon = options.beautify ? function() {
        print(";");
    } : function() {
        might_need_semicolon = true;
    };

    function force_semicolon() {
        might_need_semicolon = false;
        print(";");
    }

    function next_indent() {
        return indentation + options.indent_level;
    }

    function with_block(cont) {
        var ret;
        print("{");
        newline();
        with_indent(next_indent(), function() {
            ret = cont();
        });
        indent();
        print("}");
        return ret;
    }

    function with_parens(cont) {
        print("(");
        //XXX: still nice to have that for argument lists
        //var ret = with_indent(current_col, cont);
        var ret = cont();
        print(")");
        return ret;
    }

    function with_square(cont) {
        print("[");
        //var ret = with_indent(current_col, cont);
        var ret = cont();
        print("]");
        return ret;
    }

    function comma() {
        print(",");
        space();
    }

    function colon() {
        print(":");
        space();
    }

    var add_mapping = mappings ? function(token, name) {
        mapping_token = token;
        mapping_name = name;
    } : noop;

    function get() {
        if (might_add_newline) {
            ensure_line_len();
        }
        return OUTPUT;
    }

    function has_nlb() {
        var index = OUTPUT.lastIndexOf("\n");
        return /^ *$/.test(OUTPUT.slice(index + 1));
    }

    function prepend_comments(node) {
        var self = this;
        var start = node.start;
        if (!start) return;
        if (start.comments_before && start.comments_before._dumped === self) return;
        var comments = start.comments_before;
        if (!comments) {
            comments = start.comments_before = [];
        }
        comments._dumped = self;

        if (node instanceof AST_Exit && node.value) {
            var tw = new TreeWalker(function(node) {
                var parent = tw.parent();
                if (parent instanceof AST_Exit
                    || parent instanceof AST_Binary && parent.left === node
                    || parent.TYPE == "Call" && parent.expression === node
                    || parent instanceof AST_Conditional && parent.condition === node
                    || parent instanceof AST_Dot && parent.expression === node
                    || parent instanceof AST_Sequence && parent.expressions[0] === node
                    || parent instanceof AST_Sub && parent.expression === node
                    || parent instanceof AST_UnaryPostfix) {
                    if (!node.start) return;
                    var text = node.start.comments_before;
                    if (text && text._dumped !== self) {
                        text._dumped = self;
                        comments = comments.concat(text);
                    }
                } else {
                    return true;
                }
            });
            tw.push(node);
            node.value.walk(tw);
        }

        if (current_pos == 0) {
            if (comments.length > 0 && options.shebang && comments[0].type == "comment5") {
                print("#!" + comments.shift().value + "\n");
                indent();
            }
            var preamble = options.preamble;
            if (preamble) {
                print(preamble.replace(/\r\n?|[\n\u2028\u2029]|\s*$/g, "\n"));
            }
        }

        comments = comments.filter(comment_filter, node);
        if (comments.length == 0) return;
        var last_nlb = has_nlb();
        comments.forEach(function(c, i) {
            if (!last_nlb) {
                if (c.nlb) {
                    print("\n");
                    indent();
                    last_nlb = true;
                } else if (i > 0) {
                    space();
                }
            }
            if (/comment[134]/.test(c.type)) {
                print("//" + c.value.replace(/[@#]__PURE__/g, " ") + "\n");
                indent();
                last_nlb = true;
            } else if (c.type == "comment2") {
                print("/*" + c.value.replace(/[@#]__PURE__/g, " ") + "*/");
                last_nlb = false;
            }
        });
        if (!last_nlb) {
            if (start.nlb) {
                print("\n");
                indent();
            } else {
                space();
            }
        }
    }

    function append_comments(node, tail) {
        var self = this;
        var token = node.end;
        if (!token) return;
        var comments = token[tail ? "comments_before" : "comments_after"];
        if (!comments || comments._dumped === self) return;
        if (!(node instanceof AST_Statement || all(comments, function(c) {
            return !/comment[134]/.test(c.type);
        }))) return;
        comments._dumped = self;
        var insert = OUTPUT.length;
        comments.filter(comment_filter, node).forEach(function(c, i) {
            need_space = false;
            if (need_newline_indented) {
                print("\n");
                indent();
                need_newline_indented = false;
            } else if (c.nlb && (i > 0 || !has_nlb())) {
                print("\n");
                indent();
            } else if (i > 0 || !tail) {
                space();
            }
            if (/comment[134]/.test(c.type)) {
                print("//" + c.value.replace(/[@#]__PURE__/g, " "));
                need_newline_indented = true;
            } else if (c.type == "comment2") {
                print("/*" + c.value.replace(/[@#]__PURE__/g, " ") + "*/");
                need_space = true;
            }
        });
        if (OUTPUT.length > insert) newline_insert = insert;
    }

    var stack = [];
    return {
        get             : get,
        toString        : get,
        indent          : indent,
        indentation     : function() { return indentation; },
        current_width   : function() { return current_col - indentation; },
        should_break    : function() { return options.width && this.current_width() >= options.width; },
        has_parens      : function() { return has_parens; },
        newline         : newline,
        print           : print,
        star            : star,
        space           : space,
        comma           : comma,
        colon           : colon,
        last            : function() { return last; },
        semicolon       : semicolon,
        force_semicolon : force_semicolon,
        to_utf8         : to_utf8,
        print_name      : function(name) { print(make_name(name)); },
        print_string    : function(str, quote, escape_directive) {
            var encoded = encode_string(str, quote);
            if (escape_directive === true && encoded.indexOf("\\") === -1) {
                // Insert semicolons to break directive prologue
                if (!EXPECT_DIRECTIVE.test(OUTPUT)) {
                    force_semicolon();
                }
                force_semicolon();
            }
            print(encoded);
        },
        print_template_string_chars: function(str) {
            var encoded = encode_string(str, "`").replace(/\${/g, "\\${");
            return print(encoded.substr(1, encoded.length - 2));
        },
        encode_string   : encode_string,
        next_indent     : next_indent,
        with_indent     : with_indent,
        with_block      : with_block,
        with_parens     : with_parens,
        with_square     : with_square,
        add_mapping     : add_mapping,
        option          : function(opt) { return options[opt]; },
        prepend_comments: readonly ? noop : prepend_comments,
        append_comments : readonly || comment_filter === return_false ? noop : append_comments,
        line            : function() { return current_line; },
        col             : function() { return current_col; },
        pos             : function() { return current_pos; },
        push_node       : function(node) { stack.push(node); },
        pop_node        : function() { return stack.pop(); },
        parent          : function(n) {
            return stack[stack.length - 2 - (n || 0)];
        }
    };

}

/* -----[ code generators ]----- */

(function() {

    /* -----[ utils ]----- */

    function DEFPRINT(nodetype, generator) {
        nodetype.DEFMETHOD("_codegen", generator);
    }

    var in_directive = false;
    var active_scope = null;
    var use_asm = null;

    AST_Node.DEFMETHOD("print", function(stream, force_parens) {
        var self = this, generator = self._codegen;
        if (self instanceof AST_Scope) {
            active_scope = self;
        } else if (!use_asm && self instanceof AST_Directive && self.value == "use asm") {
            use_asm = active_scope;
        }
        function doit() {
            stream.prepend_comments(self);
            self.add_source_map(stream);
            generator(self, stream);
            stream.append_comments(self);
        }
        stream.push_node(self);
        if (force_parens || self.needs_parens(stream)) {
            stream.with_parens(doit);
        } else {
            doit();
        }
        stream.pop_node();
        if (self === use_asm) {
            use_asm = null;
        }
    });
    AST_Node.DEFMETHOD("_print", AST_Node.prototype.print);

    AST_Node.DEFMETHOD("print_to_string", function(options) {
        var s = OutputStream(options);
        this.print(s);
        return s.get();
    });

    /* -----[ PARENTHESES ]----- */

    function PARENS(nodetype, func) {
        if (Array.isArray(nodetype)) {
            nodetype.forEach(function(nodetype) {
                PARENS(nodetype, func);
            });
        } else {
            nodetype.DEFMETHOD("needs_parens", func);
        }
    }

    PARENS(AST_Node, return_false);

    // a function expression needs parens around it when it's provably
    // the first token to appear in a statement.
    PARENS(AST_Function, function(output) {
        if (!output.has_parens() && first_in_statement(output)) {
            return true;
        }

        if (output.option("webkit")) {
            var p = output.parent();
            if (p instanceof AST_PropAccess && p.expression === this) {
                return true;
            }
        }

        if (output.option("wrap_iife")) {
            var p = output.parent();
            return p instanceof AST_Call && p.expression === this;
        }

        return false;
    });

    PARENS(AST_Arrow, function(output) {
        var p = output.parent();
        return p instanceof AST_PropAccess && p.expression === this;
    });

    // same goes for an object literal, because otherwise it would be
    // interpreted as a block of code.
    PARENS(AST_Object, function(output) {
        return !output.has_parens() && first_in_statement(output);
    });

    PARENS(AST_ClassExpression, first_in_statement);

    PARENS(AST_Unary, function(output) {
        var p = output.parent();
        return p instanceof AST_PropAccess && p.expression === this
            || p instanceof AST_Call && p.expression === this
            || p instanceof AST_Binary
                && p.operator === "**"
                && this instanceof AST_UnaryPrefix
                && p.left === this
                && this.operator !== "++"
                && this.operator !== "--";
    });

    PARENS(AST_Await, function(output) {
        var p = output.parent();
        return p instanceof AST_PropAccess && p.expression === this
            || p instanceof AST_Call && p.expression === this
            || output.option("safari10") && p instanceof AST_UnaryPrefix;
    });

    PARENS(AST_Sequence, function(output) {
        var p = output.parent();
        return p instanceof AST_Call                          // (foo, bar)() or foo(1, (2, 3), 4)
            || p instanceof AST_Unary                         // !(foo, bar, baz)
            || p instanceof AST_Binary                        // 1 + (2, 3) + 4 ==> 8
            || p instanceof AST_VarDef                        // var a = (1, 2), b = a + a; ==> b == 4
            || p instanceof AST_PropAccess                    // (1, {foo:2}).foo or (1, {foo:2})["foo"] ==> 2
            || p instanceof AST_Array                         // [ 1, (2, 3), 4 ] ==> [ 1, 3, 4 ]
            || p instanceof AST_ObjectProperty                // { foo: (1, 2) }.foo ==> 2
            || p instanceof AST_Conditional                   /* (false, true) ? (a = 10, b = 20) : (c = 30)
                                                               * ==> 20 (side effect, set a := 10 and b := 20) */
            || p instanceof AST_Arrow                         // x => (x, x)
            || p instanceof AST_DefaultAssign                 // x => (x = (0, function(){}))
            || p instanceof AST_Expansion                     // [...(a, b)]
            || p instanceof AST_ForOf && this === p.object    // for (e of (foo, bar)) {}
            || p instanceof AST_Yield                         // yield (foo, bar)
            || p instanceof AST_Export                        // export default (foo, bar)
        ;
    });

    PARENS(AST_Binary, function(output) {
        var p = output.parent();
        // (foo && bar)()
        if (p instanceof AST_Call && p.expression === this)
            return true;
        // typeof (foo && bar)
        if (p instanceof AST_Unary)
            return true;
        // (foo && bar)["prop"], (foo && bar).prop
        if (p instanceof AST_PropAccess && p.expression === this)
            return true;
        // this deals with precedence: 3 * (2 + 1)
        if (p instanceof AST_Binary) {
            var po = p.operator, pp = PRECEDENCE[po];
            var so = this.operator, sp = PRECEDENCE[so];
            if (pp > sp
                || (pp == sp
                    && (this === p.right || po == "**"))) {
                return true;
            }
        }
    });

    PARENS(AST_Yield, function(output) {
        var p = output.parent();
        // (yield 1) + (yield 2)
        // a = yield 3
        if (p instanceof AST_Binary && p.operator !== "=")
            return true;
        // (yield 1)()
        // new (yield 1)()
        if (p instanceof AST_Call && p.expression === this)
            return true;
        // (yield 1) ? yield 2 : yield 3
        if (p instanceof AST_Conditional && p.condition === this)
            return true;
        // -(yield 4)
        if (p instanceof AST_Unary)
            return true;
        // (yield x).foo
        // (yield x)['foo']
        if (p instanceof AST_PropAccess && p.expression === this)
            return true;
    });

    PARENS(AST_PropAccess, function(output) {
        var p = output.parent();
        if (p instanceof AST_New && p.expression === this) {
            // i.e. new (foo.bar().baz)
            //
            // if there's one call into this subtree, then we need
            // parens around it too, otherwise the call will be
            // interpreted as passing the arguments to the upper New
            // expression.
            var parens = false;
            this.walk(new TreeWalker(function(node) {
                if (parens || node instanceof AST_Scope) return true;
                if (node instanceof AST_Call) {
                    parens = true;
                    return true;
                }
            }));
            return parens;
        }
    });

    PARENS(AST_Call, function(output) {
        var p = output.parent(), p1;
        if (p instanceof AST_New && p.expression === this
            || p instanceof AST_Export && p.is_default && this.expression instanceof AST_Function)
            return true;

        // workaround for Safari bug.
        // https://bugs.webkit.org/show_bug.cgi?id=123506
        return this.expression instanceof AST_Function
            && p instanceof AST_PropAccess
            && p.expression === this
            && (p1 = output.parent(1)) instanceof AST_Assign
            && p1.left === p;
    });

    PARENS(AST_New, function(output) {
        var p = output.parent();
        if (!need_constructor_parens(this, output)
            && (p instanceof AST_PropAccess // (new Date).getTime(), (new Date)["getTime"]()
                || p instanceof AST_Call && p.expression === this)) // (new foo)(bar)
            return true;
    });

    PARENS(AST_Number, function(output) {
        var p = output.parent();
        if (p instanceof AST_PropAccess && p.expression === this) {
            var value = this.getValue();
            if (value < 0 || /^0/.test(make_num(value))) {
                return true;
            }
        }
    });

    PARENS([ AST_Assign, AST_Conditional ], function(output) {
        var p = output.parent();
        // !(a = false) → true
        if (p instanceof AST_Unary)
            return true;
        // 1 + (a = 2) + 3 → 6, side effect setting a = 2
        if (p instanceof AST_Binary && !(p instanceof AST_Assign))
            return true;
        // (a = func)() —or— new (a = Object)()
        if (p instanceof AST_Call && p.expression === this)
            return true;
        // (a = foo) ? bar : baz
        if (p instanceof AST_Conditional && p.condition === this)
            return true;
        // (a = foo)["prop"] —or— (a = foo).prop
        if (p instanceof AST_PropAccess && p.expression === this)
            return true;
        // ({a, b} = {a: 1, b: 2}), a destructuring assignment
        if (this instanceof AST_Assign && this.left instanceof AST_Destructuring && this.left.is_array === false)
            return true;
    });

    /* -----[ PRINTERS ]----- */

    DEFPRINT(AST_Directive, function(self, output) {
        output.print_string(self.value, self.quote);
        output.semicolon();
    });

    DEFPRINT(AST_Expansion, function (self, output) {
        output.print("...");
        self.expression.print(output);
    });

    DEFPRINT(AST_Destructuring, function (self, output) {
        output.print(self.is_array ? "[" : "{");
        var len = self.names.length;
        self.names.forEach(function (name, i) {
            if (i > 0) output.comma();
            name.print(output);
            // If the final element is a hole, we need to make sure it
            // doesn't look like a trailing comma, by inserting an actual
            // trailing comma.
            if (i == len - 1 && name instanceof AST_Hole) output.comma();
        });
        output.print(self.is_array ? "]" : "}");
    });

    DEFPRINT(AST_Debugger, function(self, output) {
        output.print("debugger");
        output.semicolon();
    });

    /* -----[ statements ]----- */

    function display_body(body, is_toplevel, output, allow_directives) {
        var last = body.length - 1;
        in_directive = allow_directives;
        body.forEach(function(stmt, i) {
            if (in_directive === true && !(stmt instanceof AST_Directive ||
                stmt instanceof AST_EmptyStatement ||
                (stmt instanceof AST_SimpleStatement && stmt.body instanceof AST_String)
            )) {
                in_directive = false;
            }
            if (!(stmt instanceof AST_EmptyStatement)) {
                output.indent();
                stmt.print(output);
                if (!(i == last && is_toplevel)) {
                    output.newline();
                    if (is_toplevel) output.newline();
                }
            }
            if (in_directive === true &&
                stmt instanceof AST_SimpleStatement &&
                stmt.body instanceof AST_String
            ) {
                in_directive = false;
            }
        });
        in_directive = false;
    }

    AST_StatementWithBody.DEFMETHOD("_do_print_body", function(output) {
        force_statement(this.body, output);
    });

    DEFPRINT(AST_Statement, function(self, output) {
        self.body.print(output);
        output.semicolon();
    });
    DEFPRINT(AST_Toplevel, function(self, output) {
        display_body(self.body, true, output, true);
        output.print("");
    });
    DEFPRINT(AST_LabeledStatement, function(self, output) {
        self.label.print(output);
        output.colon();
        self.body.print(output);
    });
    DEFPRINT(AST_SimpleStatement, function(self, output) {
        self.body.print(output);
        output.semicolon();
    });
// XXX Emscripten localmod: Add a node type for a parenthesized expression so that we can retain
// Closure annotations that need a form "/**annotation*/(expression)"
    DEFPRINT(AST_ParenthesizedExpression, function(self, output) {
        output.print('(');
        self.body.print(output);
        output.print(')');
    });
// XXX End Emscripten localmod
    function print_braced_empty(self, output) {
        output.print("{");
        output.with_indent(output.next_indent(), function() {
            output.append_comments(self, true);
        });
        output.print("}");
    }
    function print_braced(self, output, allow_directives) {
        if (self.body.length > 0) {
            output.with_block(function() {
                display_body(self.body, false, output, allow_directives);
            });
        } else print_braced_empty(self, output);
    }
    DEFPRINT(AST_BlockStatement, function(self, output) {
        print_braced(self, output);
    });
    DEFPRINT(AST_EmptyStatement, function(self, output) {
        output.semicolon();
    });
    DEFPRINT(AST_Do, function(self, output) {
        output.print("do");
        output.space();
        make_block(self.body, output);
        output.space();
        output.print("while");
        output.space();
        output.with_parens(function() {
            self.condition.print(output);
        });
        output.semicolon();
    });
    DEFPRINT(AST_While, function(self, output) {
        output.print("while");
        output.space();
        output.with_parens(function() {
            self.condition.print(output);
        });
        output.space();
        self._do_print_body(output);
    });
    DEFPRINT(AST_For, function(self, output) {
        output.print("for");
        output.space();
        output.with_parens(function() {
            if (self.init) {
                if (self.init instanceof AST_Definitions) {
                    self.init.print(output);
                } else {
                    parenthesize_for_noin(self.init, output, true);
                }
                output.print(";");
                output.space();
            } else {
                output.print(";");
            }
            if (self.condition) {
                self.condition.print(output);
                output.print(";");
                output.space();
            } else {
                output.print(";");
            }
            if (self.step) {
                self.step.print(output);
            }
        });
        output.space();
        self._do_print_body(output);
    });
    DEFPRINT(AST_ForIn, function(self, output) {
        output.print("for");
        if (self.await) {
            output.space();
            output.print("await");
        }
        output.space();
        output.with_parens(function() {
            self.init.print(output);
            output.space();
            output.print(self instanceof AST_ForOf ? "of" : "in");
            output.space();
            self.object.print(output);
        });
        output.space();
        self._do_print_body(output);
    });
    DEFPRINT(AST_With, function(self, output) {
        output.print("with");
        output.space();
        output.with_parens(function() {
            self.expression.print(output);
        });
        output.space();
        self._do_print_body(output);
    });

    /* -----[ functions ]----- */
    AST_Lambda.DEFMETHOD("_do_print", function(output, nokeyword) {
        var self = this;
        if (!nokeyword) {
            if (self.async) {
                output.print("async");
                output.space();
            }
            output.print("function");
            if (self.is_generator) {
                output.star();
            }
            if (self.name) {
                output.space();
            }
        }
        if (self.name instanceof AST_Symbol) {
            self.name.print(output);
        } else if (nokeyword && self.name instanceof AST_Node) {
            output.with_square(function() {
                self.name.print(output); // Computed method name
            });
        }
        output.with_parens(function() {
            self.argnames.forEach(function(arg, i) {
                if (i) output.comma();
                arg.print(output);
            });
        });
        output.space();
        print_braced(self, output, true);
    });
    DEFPRINT(AST_Lambda, function(self, output) {
        self._do_print(output);
    });

    DEFPRINT(AST_PrefixedTemplateString, function(self, output) {
        var tag = self.prefix;
        var parenthesize_tag = tag instanceof AST_Arrow
            || tag instanceof AST_Binary
            || tag instanceof AST_Conditional
            || tag instanceof AST_Sequence
            || tag instanceof AST_Unary;
        if (parenthesize_tag) output.print("(");
        self.prefix.print(output);
        if (parenthesize_tag) output.print(")");
        self.template_string.print(output);
    });
    DEFPRINT(AST_TemplateString, function(self, output) {
        var is_tagged = output.parent() instanceof AST_PrefixedTemplateString;

        output.print("`");
        for (var i = 0; i < self.segments.length; i++) {
            if (!(self.segments[i] instanceof AST_TemplateSegment)) {
                output.print("${");
                self.segments[i].print(output);
                output.print("}");
            } else if (is_tagged) {
                output.print(self.segments[i].raw);
            } else {
                output.print_template_string_chars(self.segments[i].value);
            }
        }
        output.print("`");
    });

    AST_Arrow.DEFMETHOD("_do_print", function(output) {
        var self = this;
        var parent = output.parent();
        var needs_parens = parent instanceof AST_Binary ||
            parent instanceof AST_Unary ||
            (parent instanceof AST_Call && self === parent.expression);
        if (needs_parens) { output.print("("); }
        if (self.async) {
            output.print("async");
            output.space();
        }
        if (self.argnames.length === 1 && self.argnames[0] instanceof AST_Symbol) {
            self.argnames[0].print(output);
        } else {
            output.with_parens(function() {
                self.argnames.forEach(function(arg, i) {
                    if (i) output.comma();
                    arg.print(output);
                });
            });
        }
        output.space();
        output.print("=>");
        output.space();
        if (self.body instanceof AST_Node) {
            self.body.print(output);
        } else {
            print_braced(self, output);
        }
        if (needs_parens) { output.print(")"); }
    });

    /* -----[ exits ]----- */
    AST_Exit.DEFMETHOD("_do_print", function(output, kind) {
        output.print(kind);
        if (this.value) {
            output.space();
            this.value.print(output);
        }
        output.semicolon();
    });
    DEFPRINT(AST_Return, function(self, output) {
        self._do_print(output, "return");
    });
    DEFPRINT(AST_Throw, function(self, output) {
        self._do_print(output, "throw");
    });

    /* -----[ yield ]----- */

    DEFPRINT(AST_Yield, function(self, output) {
        var star = self.is_star ? "*" : "";
        output.print("yield" + star);
        if (self.expression) {
            output.space();
            self.expression.print(output);
        }
    });

    DEFPRINT(AST_Await, function(self, output) {
        output.print("await");
        output.space();
        var e = self.expression;
        var parens = !(
               e instanceof AST_Call
            || e instanceof AST_SymbolRef
            || e instanceof AST_PropAccess
            || e instanceof AST_Unary
            || e instanceof AST_Constant
        );
        if (parens) output.print("(");
        self.expression.print(output);
        if (parens) output.print(")");
    });

    /* -----[ loop control ]----- */
    AST_LoopControl.DEFMETHOD("_do_print", function(output, kind) {
        output.print(kind);
        if (this.label) {
            output.space();
            this.label.print(output);
        }
        output.semicolon();
    });
    DEFPRINT(AST_Break, function(self, output) {
        self._do_print(output, "break");
    });
    DEFPRINT(AST_Continue, function(self, output) {
        self._do_print(output, "continue");
    });

    /* -----[ if ]----- */
    function make_then(self, output) {
        var b = self.body;
        if (output.option("braces")
            || output.option("ie8") && b instanceof AST_Do)
            return make_block(b, output);
        // The squeezer replaces "block"-s that contain only a single
        // statement with the statement itself; technically, the AST
        // is correct, but this can create problems when we output an
        // IF having an ELSE clause where the THEN clause ends in an
        // IF *without* an ELSE block (then the outer ELSE would refer
        // to the inner IF).  This function checks for this case and
        // adds the block braces if needed.
        if (!b) return output.force_semicolon();
        while (true) {
            if (b instanceof AST_If) {
                if (!b.alternative) {
                    make_block(self.body, output);
                    return;
                }
                b = b.alternative;
            } else if (b instanceof AST_StatementWithBody) {
                b = b.body;
            } else break;
        }
        force_statement(self.body, output);
    }
    DEFPRINT(AST_If, function(self, output) {
        output.print("if");
        output.space();
        output.with_parens(function() {
            self.condition.print(output);
        });
        output.space();
        if (self.alternative) {
            make_then(self, output);
            output.space();
            output.print("else");
            output.space();
            if (self.alternative instanceof AST_If)
                self.alternative.print(output);
            else
                force_statement(self.alternative, output);
        } else {
            self._do_print_body(output);
        }
    });

    /* -----[ switch ]----- */
    DEFPRINT(AST_Switch, function(self, output) {
        output.print("switch");
        output.space();
        output.with_parens(function() {
            self.expression.print(output);
        });
        output.space();
        var last = self.body.length - 1;
        if (last < 0) print_braced_empty(self, output);
        else output.with_block(function() {
            self.body.forEach(function(branch, i) {
                output.indent(true);
                branch.print(output);
                if (i < last && branch.body.length > 0)
                    output.newline();
            });
        });
    });
    AST_SwitchBranch.DEFMETHOD("_do_print_body", function(output) {
        output.newline();
        this.body.forEach(function(stmt) {
            output.indent();
            stmt.print(output);
            output.newline();
        });
    });
    DEFPRINT(AST_Default, function(self, output) {
        output.print("default:");
        self._do_print_body(output);
    });
    DEFPRINT(AST_Case, function(self, output) {
        output.print("case");
        output.space();
        self.expression.print(output);
        output.print(":");
        self._do_print_body(output);
    });

    /* -----[ exceptions ]----- */
    DEFPRINT(AST_Try, function(self, output) {
        output.print("try");
        output.space();
        print_braced(self, output);
        if (self.bcatch) {
            output.space();
            self.bcatch.print(output);
        }
        if (self.bfinally) {
            output.space();
            self.bfinally.print(output);
        }
    });
    DEFPRINT(AST_Catch, function(self, output) {
        output.print("catch");
        if (self.argname) {
            output.space();
            output.with_parens(function() {
                self.argname.print(output);
            });
        }
        output.space();
        print_braced(self, output);
    });
    DEFPRINT(AST_Finally, function(self, output) {
        output.print("finally");
        output.space();
        print_braced(self, output);
    });

    /* -----[ var/const ]----- */
    AST_Definitions.DEFMETHOD("_do_print", function(output, kind) {
        output.print(kind);
        output.space();
        this.definitions.forEach(function(def, i) {
            if (i) output.comma();
            def.print(output);
        });
        var p = output.parent();
        var in_for = p instanceof AST_For || p instanceof AST_ForIn;
        var output_semicolon = !in_for || p && p.init !== this;
        if (output_semicolon)
            output.semicolon();
    });
    DEFPRINT(AST_Let, function(self, output) {
        self._do_print(output, "let");
    });
    DEFPRINT(AST_Var, function(self, output) {
        self._do_print(output, "var");
    });
    DEFPRINT(AST_Const, function(self, output) {
        self._do_print(output, "const");
    });
    DEFPRINT(AST_Import, function(self, output) {
        output.print("import");
        output.space();
        if (self.imported_name) {
            self.imported_name.print(output);
        }
        if (self.imported_name && self.imported_names) {
            output.print(",");
            output.space();
        }
        if (self.imported_names) {
            if (self.imported_names.length === 1 && self.imported_names[0].foreign_name.name === "*") {
                self.imported_names[0].print(output);
            } else {
                output.print("{");
                self.imported_names.forEach(function (name_import, i) {
                    output.space();
                    name_import.print(output);
                    if (i < self.imported_names.length - 1) {
                        output.print(",");
                    }
                });
                output.space();
                output.print("}");
            }
        }
        if (self.imported_name || self.imported_names) {
            output.space();
            output.print("from");
            output.space();
        }
        self.module_name.print(output);
        output.semicolon();
    });

    DEFPRINT(AST_NameMapping, function(self, output) {
        var is_import = output.parent() instanceof AST_Import;
        var definition = self.name.definition();
        var names_are_different =
            (definition && definition.mangled_name || self.name.name) !==
            self.foreign_name.name;
        if (names_are_different) {
            if (is_import) {
                output.print(self.foreign_name.name);
            } else {
                self.name.print(output);
            }
            output.space();
            output.print("as");
            output.space();
            if (is_import) {
                self.name.print(output);
            } else {
                output.print(self.foreign_name.name);
            }
        } else {
            self.name.print(output);
        }
    });

    DEFPRINT(AST_Export, function(self, output) {
        output.print("export");
        output.space();
        if (self.is_default) {
            output.print("default");
            output.space();
        }
        if (self.exported_names) {
            if (self.exported_names.length === 1 && self.exported_names[0].name.name === "*") {
                self.exported_names[0].print(output);
            } else {
                output.print("{");
                self.exported_names.forEach(function(name_export, i) {
                    output.space();
                    name_export.print(output);
                    if (i < self.exported_names.length - 1) {
                        output.print(",");
                    }
                });
                output.space();
                output.print("}");
            }
        } else if (self.exported_value) {
            self.exported_value.print(output);
        } else if (self.exported_definition) {
            self.exported_definition.print(output);
            if (self.exported_definition instanceof AST_Definitions) return;
        }
        if (self.module_name) {
            output.space();
            output.print("from");
            output.space();
            self.module_name.print(output);
        }
        if (self.exported_value
                && !(self.exported_value instanceof AST_Defun ||
                    self.exported_value instanceof AST_Function ||
                    self.exported_value instanceof AST_Class)
            || self.module_name
            || self.exported_names
        ) {
            output.semicolon();
        }
    });

    function parenthesize_for_noin(node, output, noin) {
        var parens = false;
        // need to take some precautions here:
        //    https://github.com/mishoo/UglifyJS2/issues/60
        if (noin) node.walk(new TreeWalker(function(node) {
            if (parens || node instanceof AST_Scope) return true;
            if (node instanceof AST_Binary && node.operator == "in") {
                parens = true;
                return true;
            }
        }));
        node.print(output, parens);
    }

    DEFPRINT(AST_VarDef, function(self, output) {
        self.name.print(output);
        if (self.value) {
            output.space();
            output.print("=");
            output.space();
            var p = output.parent(1);
            var noin = p instanceof AST_For || p instanceof AST_ForIn;
            parenthesize_for_noin(self.value, output, noin);
        }
    });

    /* -----[ other expressions ]----- */
    DEFPRINT(AST_Call, function(self, output) {
        self.expression.print(output);
        if (self instanceof AST_New && !need_constructor_parens(self, output))
            return;
        if (self.expression instanceof AST_Call || self.expression instanceof AST_Lambda) {
            output.add_mapping(self.start);
        }
        output.with_parens(function() {
            self.args.forEach(function(expr, i) {
                if (i) output.comma();
                expr.print(output);
            });
        });
    });
    DEFPRINT(AST_New, function(self, output) {
        output.print("new");
        output.space();
        AST_Call.prototype._codegen(self, output);
    });

    AST_Sequence.DEFMETHOD("_do_print", function(output) {
        this.expressions.forEach(function(node, index) {
            if (index > 0) {
                output.comma();
                if (output.should_break()) {
                    output.newline();
                    output.indent();
                }
            }
            node.print(output);
        });
    });
    DEFPRINT(AST_Sequence, function(self, output) {
        self._do_print(output);
        // var p = output.parent();
        // if (p instanceof AST_Statement) {
        //     output.with_indent(output.next_indent(), function(){
        //         self._do_print(output);
        //     });
        // } else {
        //     self._do_print(output);
        // }
    });
    DEFPRINT(AST_Dot, function(self, output) {
        var expr = self.expression;
        expr.print(output);
        var prop = self.property;
        if (output.option("ie8") && RESERVED_WORDS(prop)) {
            output.print("[");
            output.add_mapping(self.end);
            output.print_string(prop);
            output.print("]");
        } else {
            if (expr instanceof AST_Number && expr.getValue() >= 0) {
                if (!/[xa-f.)]/i.test(output.last())) {
                    output.print(".");
                }
            }
            output.print(".");
            // the name after dot would be mapped about here.
            output.add_mapping(self.end);
            output.print_name(prop);
        }
    });
    DEFPRINT(AST_Sub, function(self, output) {
        self.expression.print(output);
        output.print("[");
        self.property.print(output);
        output.print("]");
    });
    DEFPRINT(AST_UnaryPrefix, function(self, output) {
        var op = self.operator;
        output.print(op);
        if (/^[a-z]/i.test(op)
            || (/[+-]$/.test(op)
                && self.expression instanceof AST_UnaryPrefix
                && /^[+-]/.test(self.expression.operator))) {
            output.space();
        }
        self.expression.print(output);
    });
    DEFPRINT(AST_UnaryPostfix, function(self, output) {
        self.expression.print(output);
        output.print(self.operator);
    });
    DEFPRINT(AST_Binary, function(self, output) {
        var op = self.operator;
        self.left.print(output);
        if (op[0] == ">" /* ">>" ">>>" ">" ">=" */
            && self.left instanceof AST_UnaryPostfix
            && self.left.operator == "--") {
            // space is mandatory to avoid outputting -->
            output.print(" ");
        } else {
            // the space is optional depending on "beautify"
            output.space();
        }
        output.print(op);
        if ((op == "<" || op == "<<")
            && self.right instanceof AST_UnaryPrefix
            && self.right.operator == "!"
            && self.right.expression instanceof AST_UnaryPrefix
            && self.right.expression.operator == "--") {
            // space is mandatory to avoid outputting <!--
            output.print(" ");
        } else {
            // the space is optional depending on "beautify"
            output.space();
        }
        self.right.print(output);
    });
    DEFPRINT(AST_Conditional, function(self, output) {
        self.condition.print(output);
        output.space();
        output.print("?");
        output.space();
        self.consequent.print(output);
        output.space();
        output.colon();
        self.alternative.print(output);
    });

    /* -----[ literals ]----- */
    DEFPRINT(AST_Array, function(self, output) {
        output.with_square(function() {
            var a = self.elements, len = a.length;
            if (len > 0) output.space();
            a.forEach(function(exp, i) {
                if (i) output.comma();
                exp.print(output);
                // If the final element is a hole, we need to make sure it
                // doesn't look like a trailing comma, by inserting an actual
                // trailing comma.
                if (i === len - 1 && exp instanceof AST_Hole)
                  output.comma();
            });
            if (len > 0) output.space();
        });
    });
    DEFPRINT(AST_Object, function(self, output) {
        if (self.properties.length > 0) output.with_block(function() {
            self.properties.forEach(function(prop, i) {
                if (i) {
                    output.print(",");
                    output.newline();
                }
                output.indent();
                prop.print(output);
            });
            output.newline();
        });
        else print_braced_empty(self, output);
    });
    DEFPRINT(AST_Class, function(self, output) {
        output.print("class");
        output.space();
        if (self.name) {
            self.name.print(output);
            output.space();
        }
        if (self.extends) {
            var parens = (
                   !(self.extends instanceof AST_SymbolRef)
                && !(self.extends instanceof AST_PropAccess)
                && !(self.extends instanceof AST_ClassExpression)
                && !(self.extends instanceof AST_Function)
            );
            output.print("extends");
            if (parens) {
                output.print("(");
            } else {
                output.space();
            }
            self.extends.print(output);
            if (parens) {
                output.print(")");
            } else {
                output.space();
            }
        }
        if (self.properties.length > 0) output.with_block(function() {
            self.properties.forEach(function(prop, i) {
                if (i) {
                    output.newline();
                }
                output.indent();
                prop.print(output);
            });
            output.newline();
        });
        else output.print("{}");
    });
    DEFPRINT(AST_NewTarget, function(self, output) {
        output.print("new.target");
    });

    function print_property_name(key, quote, output) {
        if (output.option("quote_keys")) {
            output.print_string(key);
        } else if ("" + +key == key && key >= 0) {
            output.print(make_num(key));
        } else if (RESERVED_WORDS(key) ? !output.option("ie8") : is_identifier_string(key)) {
            if (quote && output.option("keep_quoted_props")) {
                output.print_string(key, quote);
            } else {
                output.print_name(key);
            }
        } else {
            output.print_string(key, quote);
        }
    }

    DEFPRINT(AST_ObjectKeyVal, function(self, output) {
        function get_name(self) {
            var def = self.definition();
            return def ? def.mangled_name || def.name : self.name;
        }

        var allowShortHand = output.option("shorthand");
        if (allowShortHand &&
            self.value instanceof AST_Symbol &&
            is_identifier_string(self.key) &&
            get_name(self.value) === self.key &&
            is_identifier(self.key)
        ) {
            print_property_name(self.key, self.quote, output);

        } else if (allowShortHand &&
            self.value instanceof AST_DefaultAssign &&
            self.value.left instanceof AST_Symbol &&
            is_identifier_string(self.key) &&
            get_name(self.value.left) === self.key
        ) {
            print_property_name(self.key, self.quote, output);
            output.space();
            output.print("=");
            output.space();
            self.value.right.print(output);
        } else {
            if (!(self.key instanceof AST_Node)) {
                print_property_name(self.key, self.quote, output);
            } else {
                output.with_square(function() {
                    self.key.print(output);
                });
            }
            output.colon();
            self.value.print(output);
        }
    });
    AST_ObjectProperty.DEFMETHOD("_print_getter_setter", function(type, output) {
        var self = this;
        if (self.static) {
            output.print("static");
            output.space();
        }
        if (type) {
            output.print(type);
            output.space();
        }
        if (self.key instanceof AST_SymbolMethod) {
            print_property_name(self.key.name, self.quote, output);
        } else {
            output.with_square(function() {
                self.key.print(output);
            });
        }
        self.value._do_print(output, true);
    });
    DEFPRINT(AST_ObjectSetter, function(self, output) {
        self._print_getter_setter("set", output);
    });
    DEFPRINT(AST_ObjectGetter, function(self, output) {
        self._print_getter_setter("get", output);
    });
    DEFPRINT(AST_ConciseMethod, function(self, output) {
        var type;
        if (self.is_generator && self.async) {
            type = "async*";
        } else if (self.is_generator) {
            type = "*";
        } else if (self.async) {
            type = "async";
        }
        self._print_getter_setter(type, output);
    });
    AST_Symbol.DEFMETHOD("_do_print", function(output) {
        var def = this.definition();
        output.print_name(def ? def.mangled_name || def.name : this.name);
    });
    DEFPRINT(AST_Symbol, function (self, output) {
        self._do_print(output);
    });
    DEFPRINT(AST_Hole, noop);
    DEFPRINT(AST_This, function(self, output) {
        output.print("this");
    });
    DEFPRINT(AST_Super, function(self, output) {
        output.print("super");
    });
    DEFPRINT(AST_Constant, function(self, output) {
        output.print(self.getValue());
    });
    DEFPRINT(AST_String, function(self, output) {
        output.print_string(self.getValue(), self.quote, in_directive);
    });
    DEFPRINT(AST_Number, function(self, output) {
        if (use_asm && self.start && self.start.raw != null) {
            output.print(self.start.raw);
        } else {
            output.print(make_num(self.getValue()));
        }
    });

    DEFPRINT(AST_RegExp, function(self, output) {
        var regexp = self.getValue();
        var str = regexp.toString();
        str = output.to_utf8(str);
        output.print(str);
        var p = output.parent();
        if (p instanceof AST_Binary && /^in/.test(p.operator) && p.left === self)
            output.print(" ");
    });

    function force_statement(stat, output) {
        if (output.option("braces")) {
            make_block(stat, output);
        } else {
            if (!stat || stat instanceof AST_EmptyStatement)
                output.force_semicolon();
            else
                stat.print(output);
        }
    }

    // self should be AST_New.  decide if we want to show parens or not.
    function need_constructor_parens(self, output) {
        // Always print parentheses with arguments
        if (self.args.length > 0) return true;

        return output.option("beautify");
    }

    function best_of(a) {
        var best = a[0], len = best.length;
        for (var i = 1; i < a.length; ++i) {
            if (a[i].length < len) {
                best = a[i];
                len = best.length;
            }
        }
        return best;
    }

    function make_num(num) {
        var str = num.toString(10), a = [ str.replace(/^0\./, ".").replace("e+", "e") ], m;
        if (Math.floor(num) === num) {
            if (num >= 0) {
                a.push("0x" + num.toString(16).toLowerCase(), // probably pointless
                       "0" + num.toString(8)); // same.
            } else {
                a.push("-0x" + (-num).toString(16).toLowerCase(), // probably pointless
                       "-0" + (-num).toString(8)); // same.
            }
            if ((m = /^(.*?)(0+)$/.exec(num))) {
                a.push(m[1] + "e" + m[2].length);
            }
        } else if ((m = /^0?\.(0+)(.*)$/.exec(num))) {
            a.push(m[2] + "e-" + (m[1].length + m[2].length),
                   str.substr(str.indexOf(".")));
        }
        return best_of(a);
    }

    function make_block(stmt, output) {
        if (!stmt || stmt instanceof AST_EmptyStatement)
            output.print("{}");
        else if (stmt instanceof AST_BlockStatement)
            stmt.print(output);
        else output.with_block(function() {
            output.indent();
            stmt.print(output);
            output.newline();
        });
    }

    /* -----[ source map generators ]----- */

    function DEFMAP(nodetype, generator) {
        nodetype.forEach(function(nodetype) {
            nodetype.DEFMETHOD("add_source_map", generator);
        });
    }

    DEFMAP([
        // We could easily add info for ALL nodes, but it seems to me that
        // would be quite wasteful, hence this noop in the base class.
        AST_Node,
        // since the label symbol will mark it
        AST_LabeledStatement,
        AST_Toplevel,
    ], noop);

    // XXX: I'm not exactly sure if we need it for all of these nodes,
    // or if we should add even more.
    DEFMAP([
        AST_Array,
        AST_BlockStatement,
        AST_Catch,
        AST_Class,
        AST_Constant,
        AST_Debugger,
        AST_Definitions,
        AST_Directive,
        AST_Finally,
        AST_Jump,
        AST_Lambda,
        AST_New,
        AST_Object,
        AST_StatementWithBody,
        AST_Symbol,
        AST_Switch,
        AST_SwitchBranch,
        AST_Try,
    ], function(output) {
        output.add_mapping(this.start);
    });

    DEFMAP([
        AST_ObjectGetter,
        AST_ObjectSetter,
    ], function(output) {
        output.add_mapping(this.start, this.key.name);
    });

    DEFMAP([ AST_ObjectProperty ], function(output) {
        output.add_mapping(this.start, this.key);
    });
})();
/***********************************************************************

  A JavaScript tokenizer / parser / beautifier / compressor.
  https://github.com/mishoo/UglifyJS2

  -------------------------------- (C) ---------------------------------

                           Author: Mihai Bazon
                         <mihai.bazon@gmail.com>
                       http://mihai.bazon.net/blog

  Distributed under the BSD license:

    Copyright 2012 (c) Mihai Bazon <mihai.bazon@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

        * Redistributions of source code must retain the above
          copyright notice, this list of conditions and the following
          disclaimer.

        * Redistributions in binary form must reproduce the above
          copyright notice, this list of conditions and the following
          disclaimer in the documentation and/or other materials
          provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

 ***********************************************************************/

"use strict";

(function() {

    var normalize_directives = function(body) {
        var in_directive = true;

        for (var i = 0; i < body.length; i++) {
            if (in_directive && body[i] instanceof AST_Statement && body[i].body instanceof AST_String) {
                body[i] = new AST_Directive({
                    start: body[i].start,
                    end: body[i].end,
                    value: body[i].body.value
                });
            } else if (in_directive && !(body[i] instanceof AST_Statement && body[i].body instanceof AST_String)) {
                in_directive = false;
            }
        }

        return body;
    };

    var MOZ_TO_ME = {
        Program: function(M) {
            return new AST_Toplevel({
                start: my_start_token(M),
                end: my_end_token(M),
                body: normalize_directives(M.body.map(from_moz))
            });
        },
        ArrayPattern: function(M) {
            return new AST_Destructuring({
                start: my_start_token(M),
                end: my_end_token(M),
                names: M.elements.map(function(elm) {
                    if (elm === null) {
                        return new AST_Hole();
                    }
                    return from_moz(elm);
                }),
                is_array: true
            });
        },
        ObjectPattern: function(M) {
            return new AST_Destructuring({
                start: my_start_token(M),
                end: my_end_token(M),
                names: M.properties.map(from_moz),
                is_array: false
            });
        },
        AssignmentPattern: function(M) {
            return new AST_Binary({
                start: my_start_token(M),
                end: my_end_token(M),
                left: from_moz(M.left),
                operator: "=",
                right: from_moz(M.right)
            });
        },
        SpreadElement: function(M) {
            return new AST_Expansion({
                start: my_start_token(M),
                end: my_end_token(M),
                expression: from_moz(M.argument)
            });
        },
        RestElement: function(M) {
            return new AST_Expansion({
                start: my_start_token(M),
                end: my_end_token(M),
                expression: from_moz(M.argument)
            });
        },
        TemplateElement: function(M) {
            return new AST_TemplateSegment({
                start: my_start_token(M),
                end: my_end_token(M),
                value: M.value.cooked,
                raw: M.value.raw
            });
        },
        TemplateLiteral: function(M) {
            var segments = [];
            for (var i = 0; i < M.quasis.length; i++) {
                segments.push(from_moz(M.quasis[i]));
                if (M.expressions[i]) {
                    segments.push(from_moz(M.expressions[i]));
                }
            }
            return new AST_TemplateString({
                start: my_start_token(M),
                end: my_end_token(M),
                segments: segments
            });
        },
        TaggedTemplateExpression: function(M) {
            return new AST_PrefixedTemplateString({
                start: my_start_token(M),
                end: my_end_token(M),
                template_string: from_moz(M.quasi),
                prefix: from_moz(M.tag)
            });
        },
        FunctionDeclaration: function(M) {
            return new AST_Defun({
                start: my_start_token(M),
                end: my_end_token(M),
                name: from_moz(M.id),
                argnames: M.params.map(from_moz),
                is_generator: M.generator,
                async: M.async,
                body: normalize_directives(from_moz(M.body).body)
            });
        },
        FunctionExpression: function(M) {
            return new AST_Function({
                start: my_start_token(M),
                end: my_end_token(M),
                name: from_moz(M.id),
                argnames: M.params.map(from_moz),
                is_generator: M.generator,
                async: M.async,
                body: normalize_directives(from_moz(M.body).body)
            });
        },
        ArrowFunctionExpression: function(M) {
            return new AST_Arrow({
                start: my_start_token(M),
                end: my_end_token(M),
                argnames: M.params.map(from_moz),
                body: from_moz(M.body),
                async: M.async,
            });
        },
        ExpressionStatement: function(M) {
            return new AST_SimpleStatement({
                start: my_start_token(M),
                end: my_end_token(M),
                body: from_moz(M.expression)
            });
        },
// XXX Emscripten localmod: Add a node type for a parenthesized expression so that we can retain
// Closure annotations that need a form "/**annotation*/(expression)"
        ParenthesizedExpression: function(M) {
            return new AST_ParenthesizedExpression({
                start: my_start_token(M),
                end: my_end_token(M),
                body: from_moz(M.expression)
            });
        },
// XXX End Emscripten localmod
        TryStatement: function(M) {
            var handlers = M.handlers || [M.handler];
            if (handlers.length > 1 || M.guardedHandlers && M.guardedHandlers.length) {
                throw new Error("Multiple catch clauses are not supported.");
            }
            return new AST_Try({
                start    : my_start_token(M),
                end      : my_end_token(M),
                body     : from_moz(M.block).body,
                bcatch   : from_moz(handlers[0]),
                bfinally : M.finalizer ? new AST_Finally(from_moz(M.finalizer)) : null
            });
        },
        Property: function(M) {
            var key = M.key;
            var args = {
                start    : my_start_token(key || M.value),
                end      : my_end_token(M.value),
                key      : key.type == "Identifier" ? key.name : key.value,
                value    : from_moz(M.value)
            };
            if (M.computed) {
                args.key = from_moz(M.key);
            }
            if (M.method) {
                args.is_generator = M.value.generator;
                args.async = M.value.async;
                if (!M.computed) {
                    args.key = new AST_SymbolMethod({ name: args.key });
                } else {
                    args.key = from_moz(M.key);
                }
                return new AST_ConciseMethod(args);
            }
            if (M.kind == "init") {
                if (key.type != "Identifier" && key.type != "Literal") {
                    args.key = from_moz(key);
                }
                return new AST_ObjectKeyVal(args);
            }
            if (typeof args.key === "string" || typeof args.key === "number") {
                args.key = new AST_SymbolMethod({
                    name: args.key
                });
            }
            args.value = new AST_Accessor(args.value);
            if (M.kind == "get") return new AST_ObjectGetter(args);
            if (M.kind == "set") return new AST_ObjectSetter(args);
            if (M.kind == "method") {
                args.async = M.value.async;
                args.is_generator = M.value.generator;
                args.quote = M.computed ? "\"" : null;
                return new AST_ConciseMethod(args);
            }
        },
        MethodDefinition: function(M) {
            var args = {
                start    : my_start_token(M),
                end      : my_end_token(M),
                key      : M.computed ? from_moz(M.key) : new AST_SymbolMethod({ name: M.key.name || M.key.value }),
                value    : from_moz(M.value),
                static   : M.static,
            };
            if (M.kind == "get") {
                return new AST_ObjectGetter(args);
            }
            if (M.kind == "set") {
                return new AST_ObjectSetter(args);
            }
            args.is_generator = M.value.generator;
            args.async = M.value.async;
            return new AST_ConciseMethod(args);
        },
        ArrayExpression: function(M) {
            return new AST_Array({
                start    : my_start_token(M),
                end      : my_end_token(M),
                elements : M.elements.map(function(elem) {
                    return elem === null ? new AST_Hole() : from_moz(elem);
                })
            });
        },
        ObjectExpression: function(M) {
            return new AST_Object({
                start      : my_start_token(M),
                end        : my_end_token(M),
                properties : M.properties.map(function(prop) {
                    if (prop.type === "SpreadElement") {
                        return from_moz(prop);
                    }
                    prop.type = "Property";
                    // XXX EMSCRIPTEN preserve quoted properties
                    // https://github.com/mishoo/UglifyJS2/pull/3323
                    var ret = from_moz(prop);
                    if (prop.key.type === "Literal" &&
                        (prop.key.raw[0] === '"' || prop.key.raw[0] === "'")) {
                        ret.quote = true;
                    }
                    return ret;
                })
            });
        },
        SequenceExpression: function(M) {
            return new AST_Sequence({
                start      : my_start_token(M),
                end        : my_end_token(M),
                expressions: M.expressions.map(from_moz)
            });
        },
        MemberExpression: function(M) {
            return new (M.computed ? AST_Sub : AST_Dot)({
                start      : my_start_token(M),
                end        : my_end_token(M),
                property   : M.computed ? from_moz(M.property) : M.property.name,
                expression : from_moz(M.object)
            });
        },
        SwitchCase: function(M) {
            return new (M.test ? AST_Case : AST_Default)({
                start      : my_start_token(M),
                end        : my_end_token(M),
                expression : from_moz(M.test),
                body       : M.consequent.map(from_moz)
            });
        },
        VariableDeclaration: function(M) {
            return new (M.kind === "const" ? AST_Const :
                        M.kind === "let" ? AST_Let : AST_Var)({
                start       : my_start_token(M),
                end         : my_end_token(M),
                definitions : M.declarations.map(from_moz)
            });
        },
        
        ImportDeclaration: function(M) {
            var imported_name = null;
            var imported_names = null;
            M.specifiers.forEach(function (specifier) {
                if (specifier.type === "ImportSpecifier") {
                    if (!imported_names) { imported_names = []; }
                    imported_names.push(new AST_NameMapping({
                        start: my_start_token(specifier),
                        end: my_end_token(specifier),
                        foreign_name: from_moz(specifier.imported),
                        name: from_moz(specifier.local)
                    }));
                } else if (specifier.type === "ImportDefaultSpecifier") {
                    imported_name = from_moz(specifier.local);
                } else if (specifier.type === "ImportNamespaceSpecifier") {
                    if (!imported_names) { imported_names = []; }
                    imported_names.push(new AST_NameMapping({
                        start: my_start_token(specifier),
                        end: my_end_token(specifier),
                        foreign_name: new AST_SymbolImportForeign({ name: "*" }),
                        name: from_moz(specifier.local)
                    }));
                }
            });
            return new AST_Import({
                start       : my_start_token(M),
                end         : my_end_token(M),
                imported_name: imported_name,
                imported_names : imported_names,
                module_name : from_moz(M.source)
            });
        },
        ExportAllDeclaration: function(M) {
            return new AST_Export({
                start: my_start_token(M),
                end: my_end_token(M),
                exported_names: [
                    new AST_NameMapping({
                        name: new AST_SymbolExportForeign({ name: "*" }),
                        foreign_name: new AST_SymbolExportForeign({ name: "*" })
                    })
                ],
                module_name: from_moz(M.source)
            });
        },
        ExportNamedDeclaration: function(M) {
            return new AST_Export({
                start: my_start_token(M),
                end: my_end_token(M),
                exported_definition: from_moz(M.declaration),
                exported_names: M.specifiers && M.specifiers.length ? M.specifiers.map(function (specifier) {
                    return new AST_NameMapping({
                        foreign_name: from_moz(specifier.exported),
                        name: from_moz(specifier.local)
                    });
                }) : null,
                module_name: from_moz(M.source)
            });
        },
        ExportDefaultDeclaration: function(M) {
            return new AST_Export({
                start: my_start_token(M),
                end: my_end_token(M),
                exported_value: from_moz(M.declaration),
                is_default: true
            });
        },
        Literal: function(M) {
            var val = M.value, args = {
                start  : my_start_token(M),
                end    : my_end_token(M)
            };
            if (val === null) return new AST_Null(args);
            var rx = M.regex;
            if (rx && rx.pattern) {
                // RegExpLiteral as per ESTree AST spec
                args.value = new RegExp(rx.pattern, rx.flags);
                var raw = args.value.toString();
                args.value.raw_source = rx.flags
                    ? raw.substring(0, raw.length - rx.flags.length) + rx.flags
                    : raw;
                return new AST_RegExp(args);
            } else if (rx) {
                // support legacy RegExp
                args.value = M.regex && M.raw ? M.raw : val;
                return new AST_RegExp(args);
            }
            switch (typeof val) {
              case "string":
                args.value = val;
                return new AST_String(args);
              case "number":
                args.value = val;
                return new AST_Number(args);
              case "boolean":
                return new (val ? AST_True : AST_False)(args);
            }
        },
        MetaProperty: function(M) {
            if (M.meta.name === "new" && M.property.name === "target") {
                return new AST_NewTarget({
                    start: my_start_token(M),
                    end: my_end_token(M)
                });
            }
        },
        Identifier: function(M) {
            var p = FROM_MOZ_STACK[FROM_MOZ_STACK.length - 2];
            return new (  p.type == "LabeledStatement" ? AST_Label
                        : p.type == "VariableDeclarator" && p.id === M ? (p.kind == "const" ? AST_SymbolConst : p.kind == "let" ? AST_SymbolLet : AST_SymbolVar)
                        : /Import.*Specifier/.test(p.type) ? (p.local === M ? AST_SymbolImport : AST_SymbolImportForeign)
                        : p.type == "ExportSpecifier" ? (p.local === M ? AST_SymbolExport : AST_SymbolExportForeign)
                        : p.type == "FunctionExpression" ? (p.id === M ? AST_SymbolLambda : AST_SymbolFunarg)
                        : p.type == "FunctionDeclaration" ? (p.id === M ? AST_SymbolDefun : AST_SymbolFunarg)
                        : p.type == "ArrowFunctionExpression" ? (p.params.indexOf(M) !== -1) ? AST_SymbolFunarg : AST_SymbolRef
                        : p.type == "ClassExpression" ? (p.id === M ? AST_SymbolClass : AST_SymbolRef)
                        : p.type == "Property" ? (p.key === M && p.computed || p.value === M ? AST_SymbolRef : AST_SymbolMethod)
                        : p.type == "ClassDeclaration" ? (p.id === M ? AST_SymbolDefClass : AST_SymbolRef)
                        : p.type == "MethodDefinition" ? (p.computed ? AST_SymbolRef : AST_SymbolMethod)
                        : p.type == "CatchClause" ? AST_SymbolCatch
                        : p.type == "BreakStatement" || p.type == "ContinueStatement" ? AST_LabelRef
                        : AST_SymbolRef)({
                            start : my_start_token(M),
                            end   : my_end_token(M),
                            name  : M.name
                        });
        }
    };

    MOZ_TO_ME.UpdateExpression =
    MOZ_TO_ME.UnaryExpression = function To_Moz_Unary(M) {
        var prefix = "prefix" in M ? M.prefix
            : M.type == "UnaryExpression" ? true : false;
        return new (prefix ? AST_UnaryPrefix : AST_UnaryPostfix)({
            start      : my_start_token(M),
            end        : my_end_token(M),
            operator   : M.operator,
            expression : from_moz(M.argument)
        });
    };

    MOZ_TO_ME.ClassDeclaration =
    MOZ_TO_ME.ClassExpression = function From_Moz_Class(M) {
        return new (M.type === "ClassDeclaration" ? AST_DefClass : AST_ClassExpression)({
            start    : my_start_token(M),
            end      : my_end_token(M),
            name     : from_moz(M.id),
            extends  : from_moz(M.superClass),
            properties: M.body.body.map(from_moz)
        });
    };

    map("EmptyStatement", AST_EmptyStatement);
    map("BlockStatement", AST_BlockStatement, "body@body");
    map("IfStatement", AST_If, "test>condition, consequent>body, alternate>alternative");
    map("LabeledStatement", AST_LabeledStatement, "label>label, body>body");
    map("BreakStatement", AST_Break, "label>label");
    map("ContinueStatement", AST_Continue, "label>label");
    map("WithStatement", AST_With, "object>expression, body>body");
    map("SwitchStatement", AST_Switch, "discriminant>expression, cases@body");
    map("ReturnStatement", AST_Return, "argument>value");
    map("ThrowStatement", AST_Throw, "argument>value");
    map("WhileStatement", AST_While, "test>condition, body>body");
    map("DoWhileStatement", AST_Do, "test>condition, body>body");
    map("ForStatement", AST_For, "init>init, test>condition, update>step, body>body");
    map("ForInStatement", AST_ForIn, "left>init, right>object, body>body");
    map("ForOfStatement", AST_ForOf, "left>init, right>object, body>body, await=await");
    map("AwaitExpression", AST_Await, "argument>expression");
    map("YieldExpression", AST_Yield, "argument>expression, delegate=is_star");
    map("DebuggerStatement", AST_Debugger);
    map("VariableDeclarator", AST_VarDef, "id>name, init>value");
    map("CatchClause", AST_Catch, "param>argname, body%body");

    map("ThisExpression", AST_This);
    map("Super", AST_Super);
    map("BinaryExpression", AST_Binary, "operator=operator, left>left, right>right");
    map("LogicalExpression", AST_Binary, "operator=operator, left>left, right>right");
    map("AssignmentExpression", AST_Assign, "operator=operator, left>left, right>right");
    map("ConditionalExpression", AST_Conditional, "test>condition, consequent>consequent, alternate>alternative");
    map("NewExpression", AST_New, "callee>expression, arguments@args");
    map("CallExpression", AST_Call, "callee>expression, arguments@args");

    def_to_moz(AST_Toplevel, function To_Moz_Program(M) {
        return to_moz_scope("Program", M);
    });

    def_to_moz(AST_Expansion, function To_Moz_Spread(M, parent) {
        return {
            type: to_moz_in_destructuring() ? "RestElement" : "SpreadElement",
            argument: to_moz(M.expression)
        };
    });

    def_to_moz(AST_PrefixedTemplateString, function To_Moz_TaggedTemplateExpression(M) {
        return {
            type: "TaggedTemplateExpression",
            tag: to_moz(M.prefix),
            quasi: to_moz(M.template_string)
        };
    });

    def_to_moz(AST_TemplateString, function To_Moz_TemplateLiteral(M) {
        var quasis = [];
        var expressions = [];
        for (var i = 0; i < M.segments.length; i++) {
            if (i % 2 !== 0) {
                expressions.push(to_moz(M.segments[i]));
            } else {
                quasis.push({
                    type: "TemplateElement",
                    value: {
                        raw: M.segments[i].raw,
                        cooked: M.segments[i].value
                    },
                    tail: i === M.segments.length - 1
                });
            }
        }
        return {
            type: "TemplateLiteral",
            quasis: quasis,
            expressions: expressions
        };
    });

    def_to_moz(AST_Defun, function To_Moz_FunctionDeclaration(M) {
        return {
            type: "FunctionDeclaration",
            id: to_moz(M.name),
            params: M.argnames.map(to_moz),
            generator: M.is_generator,
            async: M.async,
            body: to_moz_scope("BlockStatement", M)
        };
    });

    def_to_moz(AST_Function, function To_Moz_FunctionExpression(M, parent) {
        var is_generator = parent.is_generator !== undefined ?
            parent.is_generator : M.is_generator;
        return {
            type: "FunctionExpression",
            id: to_moz(M.name),
            params: M.argnames.map(to_moz),
            generator: is_generator,
            async: M.async,
            body: to_moz_scope("BlockStatement", M)
        };
    });

    def_to_moz(AST_Arrow, function To_Moz_ArrowFunctionExpression(M) {
        var body = M.body instanceof Array ? {
            type: "BlockStatement",
            body: M.body.map(to_moz)
        } : to_moz(M.body);
        return {
            type: "ArrowFunctionExpression",
            params: M.argnames.map(to_moz),
            async: M.async,
            body: body
        };
    });

    def_to_moz(AST_Destructuring, function To_Moz_ObjectPattern(M) {
        if (M.is_array) {
            return {
                type: "ArrayPattern",
                elements: M.names.map(to_moz)
            };
        }
        return {
            type: "ObjectPattern",
            properties: M.names.map(to_moz)
        };
    });

    def_to_moz(AST_Directive, function To_Moz_Directive(M) {
        return {
            type: "ExpressionStatement",
            expression: {
                type: "Literal",
                value: M.value
            }
        };
    });

    def_to_moz(AST_SimpleStatement, function To_Moz_ExpressionStatement(M) {
        return {
            type: "ExpressionStatement",
            expression: to_moz(M.body)
        };
    });

    def_to_moz(AST_SwitchBranch, function To_Moz_SwitchCase(M) {
        return {
            type: "SwitchCase",
            test: to_moz(M.expression),
            consequent: M.body.map(to_moz)
        };
    });

    def_to_moz(AST_Try, function To_Moz_TryStatement(M) {
        return {
            type: "TryStatement",
            block: to_moz_block(M),
            handler: to_moz(M.bcatch),
            guardedHandlers: [],
            finalizer: to_moz(M.bfinally)
        };
    });

    def_to_moz(AST_Catch, function To_Moz_CatchClause(M) {
        return {
            type: "CatchClause",
            param: to_moz(M.argname),
            guard: null,
            body: to_moz_block(M)
        };
    });

    def_to_moz(AST_Definitions, function To_Moz_VariableDeclaration(M) {
        return {
            type: "VariableDeclaration",
            kind:
                M instanceof AST_Const ? "const" :
                M instanceof AST_Let ? "let" : "var",
            declarations: M.definitions.map(to_moz)
        };
    });

    def_to_moz(AST_Export, function To_Moz_ExportDeclaration(M) {
        if (M.exported_names) {
            if (M.exported_names[0].name.name === "*") {
                return {
                    type: "ExportAllDeclaration",
                    source: to_moz(M.module_name)
                };
            }
            return {
                type: "ExportNamedDeclaration",
                specifiers: M.exported_names.map(function (name_mapping) {
                    return {
                        type: "ExportSpecifier",
                        exported: to_moz(name_mapping.foreign_name),
                        local: to_moz(name_mapping.name)
                    };
                }),
                declaration: to_moz(M.exported_definition),
                source: to_moz(M.module_name)
            };
        }
        return {
            type: M.is_default ? "ExportDefaultDeclaration" : "ExportNamedDeclaration",
            declaration: to_moz(M.exported_value || M.exported_definition)
        };
    });

    def_to_moz(AST_Import, function To_Moz_ImportDeclaration(M) {
        var specifiers = [];
        if (M.imported_name) {
            specifiers.push({
                type: "ImportDefaultSpecifier",
                local: to_moz(M.imported_name)
            });
        }
        if (M.imported_names && M.imported_names[0].foreign_name.name === "*") {
            specifiers.push({
                type: "ImportNamespaceSpecifier",
                local: to_moz(M.imported_names[0].name)
            });
        } else if (M.imported_names) {
            M.imported_names.forEach(function(name_mapping) {
                specifiers.push({
                    type: "ImportSpecifier",
                    local: to_moz(name_mapping.name),
                    imported: to_moz(name_mapping.foreign_name)
                });
            });
        }
        return {
            type: "ImportDeclaration",
            specifiers: specifiers,
            source: to_moz(M.module_name)
        };
    });

    def_to_moz(AST_Sequence, function To_Moz_SequenceExpression(M) {
        return {
            type: "SequenceExpression",
            expressions: M.expressions.map(to_moz)
        };
    });

    def_to_moz(AST_PropAccess, function To_Moz_MemberExpression(M) {
        var isComputed = M instanceof AST_Sub;
        return {
            type: "MemberExpression",
            object: to_moz(M.expression),
            computed: isComputed,
            property: isComputed ? to_moz(M.property) : {type: "Identifier", name: M.property}
        };
    });

    def_to_moz(AST_Unary, function To_Moz_Unary(M) {
        return {
            type: M.operator == "++" || M.operator == "--" ? "UpdateExpression" : "UnaryExpression",
            operator: M.operator,
            prefix: M instanceof AST_UnaryPrefix,
            argument: to_moz(M.expression)
        };
    });

    def_to_moz(AST_Binary, function To_Moz_BinaryExpression(M) {
        if (M.operator == "=" && to_moz_in_destructuring()) {
            return {
                type: "AssignmentPattern",
                left: to_moz(M.left),
                right: to_moz(M.right)
            };
        }
        return {
            type: M.operator == "&&" || M.operator == "||" ? "LogicalExpression" : "BinaryExpression",
            left: to_moz(M.left),
            operator: M.operator,
            right: to_moz(M.right)
        };
    });

    def_to_moz(AST_Array, function To_Moz_ArrayExpression(M) {
        return {
            type: "ArrayExpression",
            elements: M.elements.map(to_moz)
        };
    });

    def_to_moz(AST_Object, function To_Moz_ObjectExpression(M) {
        return {
            type: "ObjectExpression",
            properties: M.properties.map(to_moz)
        };
    });

    def_to_moz(AST_ObjectProperty, function To_Moz_Property(M, parent) {
        var key = M.key instanceof AST_Node ? to_moz(M.key) : {
            type: "Identifier",
            value: M.key
        };
        if (typeof M.key === "number") {
            key = {
                type: "Literal",
                value: Number(M.key)
            };
        }
        if (typeof M.key === "string") {
            key = {
                type: "Identifier",
                name: M.key
            };
        }
        var kind;
        var string_or_num = typeof M.key === "string" || typeof M.key === "number";
        var computed = string_or_num ? false : !(M.key instanceof AST_Symbol) || M.key instanceof AST_SymbolRef;
        if (M instanceof AST_ObjectKeyVal) {
            kind = "init";
            computed = !string_or_num;
        } else
        if (M instanceof AST_ObjectGetter) {
            kind = "get";
        } else
        if (M instanceof AST_ObjectSetter) {
            kind = "set";
        }
        if (parent instanceof AST_Class) {
            return {
                type: "MethodDefinition",
                computed: computed,
                kind: kind,
                static: M.static,
                key: to_moz(M.key),
                value: to_moz(M.value)
            };
        }
        return {
            type: "Property",
            computed: computed,
            kind: kind,
            key: key,
            value: to_moz(M.value)
        };
    });

    def_to_moz(AST_ConciseMethod, function To_Moz_MethodDefinition(M, parent) {
        if (parent instanceof AST_Object) {
            return {
                type: "Property",
                computed: !(M.key instanceof AST_Symbol) || M.key instanceof AST_SymbolRef,
                kind: "init",
                method: true,
                shorthand: false,
                key: to_moz(M.key),
                value: to_moz(M.value)
            };
        }
        return {
            type: "MethodDefinition",
            computed: !(M.key instanceof AST_Symbol) || M.key instanceof AST_SymbolRef,
            kind: M.key === "constructor" ? "constructor" : "method",
            static: M.static,
            key: to_moz(M.key),
            value: to_moz(M.value)
        };
    });

    def_to_moz(AST_Class, function To_Moz_Class(M) {
        var type = M instanceof AST_ClassExpression ? "ClassExpression" : "ClassDeclaration";
        return {
            type: type,
            superClass: to_moz(M.extends),
            id: M.name ? to_moz(M.name) : null,
            body: {
                type: "ClassBody",
                body: M.properties.map(to_moz)
            }
        };
    });

    def_to_moz(AST_NewTarget, function To_Moz_MetaProperty(M) {
        return {
            type: "MetaProperty",
            meta: {
                type: "Identifier",
                name: "new"
            },
            property: {
                type: "Identifier",
                name: "target"
            }
        };
    });

    def_to_moz(AST_Symbol, function To_Moz_Identifier(M, parent) {
        if (M instanceof AST_SymbolMethod && parent.quote) {
            return {
                type: "Literal",
                value: M.name
            };
        }
        var def = M.definition();
        return {
            type: "Identifier",
            name: def ? def.mangled_name || def.name : M.name
        };
    });

    def_to_moz(AST_RegExp, function To_Moz_RegExpLiteral(M) {
        var pattern = M.value.source;
        var flags = M.value.toString().match(/[gimuys]*$/)[0];
        return {
            type: "Literal",
            value: new RegExp(pattern, flags),
            raw: M.value.raw_source,
            regex: {
                pattern: pattern,
                flags: flags,
            }
        };
    });

    def_to_moz(AST_Constant, function To_Moz_Literal(M) {
        var value = M.value;
        if (typeof value === "number" && (value < 0 || (value === 0 && 1 / value < 0))) {
            return {
                type: "UnaryExpression",
                operator: "-",
                prefix: true,
                argument: {
                    type: "Literal",
                    value: -value,
                    raw: M.start.raw
                }
            };
        }
        return {
            type: "Literal",
            value: value,
            raw: M.start.raw
        };
    });

    def_to_moz(AST_Atom, function To_Moz_Atom(M) {
        return {
            type: "Identifier",
            name: String(M.value)
        };
    });

    AST_Boolean.DEFMETHOD("to_mozilla_ast", AST_Constant.prototype.to_mozilla_ast);
    AST_Null.DEFMETHOD("to_mozilla_ast", AST_Constant.prototype.to_mozilla_ast);
    AST_Hole.DEFMETHOD("to_mozilla_ast", function To_Moz_ArrayHole() { return null; });

    AST_Block.DEFMETHOD("to_mozilla_ast", AST_BlockStatement.prototype.to_mozilla_ast);
    AST_Lambda.DEFMETHOD("to_mozilla_ast", AST_Function.prototype.to_mozilla_ast);

    /* -----[ tools ]----- */

    function raw_token(moznode) {
        if (moznode.type == "Literal") {
            return moznode.raw != null ? moznode.raw : moznode.value + "";
        }
    }

    function my_start_token(moznode) {
        var loc = moznode.loc, start = loc && loc.start;
        var range = moznode.range;
        return new AST_Token({
            file    : loc && loc.source,
            line    : start && start.line,
            col     : start && start.column,
            pos     : range ? range[0] : moznode.start,
            endline : start && start.line,
            endcol  : start && start.column,
            endpos  : range ? range[0] : moznode.start,
            raw     : raw_token(moznode),
        });
    }

    function my_end_token(moznode) {
        var loc = moznode.loc, end = loc && loc.end;
        var range = moznode.range;
        return new AST_Token({
            file    : loc && loc.source,
            line    : end && end.line,
            col     : end && end.column,
            pos     : range ? range[1] : moznode.end,
            endline : end && end.line,
            endcol  : end && end.column,
            endpos  : range ? range[1] : moznode.end,
            raw     : raw_token(moznode),
        });
    }

    function map(moztype, mytype, propmap) {
        var moz_to_me = "function From_Moz_" + moztype + "(M){\n";
        moz_to_me += "return new U2." + mytype.name + "({\n" +
            "start: my_start_token(M),\n" +
            "end: my_end_token(M)";

        var me_to_moz = "function To_Moz_" + moztype + "(M){\n";
        me_to_moz += "return {\n" +
            "type: " + JSON.stringify(moztype);

        if (propmap) propmap.split(/\s*,\s*/).forEach(function(prop) {
            var m = /([a-z0-9$_]+)(=|@|>|%)([a-z0-9$_]+)/i.exec(prop);
            if (!m) throw new Error("Can't understand property map: " + prop);
            var moz = m[1], how = m[2], my = m[3];
            moz_to_me += ",\n" + my + ": ";
            me_to_moz += ",\n" + moz + ": ";
            switch (how) {
                case "@":
                    moz_to_me += "M." + moz + ".map(from_moz)";
                    me_to_moz += "M." +  my + ".map(to_moz)";
                    break;
                case ">":
                    moz_to_me += "from_moz(M." + moz + ")";
                    me_to_moz += "to_moz(M." + my + ")";
                    break;
                case "=":
                    moz_to_me += "M." + moz;
                    me_to_moz += "M." + my;
                    break;
                case "%":
                    moz_to_me += "from_moz(M." + moz + ").body";
                    me_to_moz += "to_moz_block(M)";
                    break;
                default:
                    throw new Error("Can't understand operator in propmap: " + prop);
            }
        });

        moz_to_me += "\n})\n}";
        me_to_moz += "\n}\n}";

        //moz_to_me = parse(moz_to_me).print_to_string({ beautify: true });
        //me_to_moz = parse(me_to_moz).print_to_string({ beautify: true });
        //console.log(moz_to_me);

        moz_to_me = new Function("U2", "my_start_token", "my_end_token", "from_moz", "return(" + moz_to_me + ")")(
            exports, my_start_token, my_end_token, from_moz
        );
        me_to_moz = new Function("to_moz", "to_moz_block", "to_moz_scope", "return(" + me_to_moz + ")")(
            to_moz, to_moz_block, to_moz_scope
        );
        MOZ_TO_ME[moztype] = moz_to_me;
        def_to_moz(mytype, me_to_moz);
    }

    var FROM_MOZ_STACK = null;

    function from_moz(node) {
        FROM_MOZ_STACK.push(node);
        var ret = node != null ? MOZ_TO_ME[node.type](node) : null;
        FROM_MOZ_STACK.pop();
        return ret;
    }

    AST_Node.from_mozilla_ast = function(node) {
        var save_stack = FROM_MOZ_STACK;
        FROM_MOZ_STACK = [];
        var ast = from_moz(node);
        FROM_MOZ_STACK = save_stack;
        return ast;
    };

    function set_moz_loc(mynode, moznode, myparent) {
        var start = mynode.start;
        var end = mynode.end;
        if (start.pos != null && end.endpos != null) {
            moznode.range = [start.pos, end.endpos];
        }
        if (start.line) {
            moznode.loc = {
                start: {line: start.line, column: start.col},
                end: end.endline ? {line: end.endline, column: end.endcol} : null
            };
            if (start.file) {
                moznode.loc.source = start.file;
            }
        }
        return moznode;
    }

    function def_to_moz(mytype, handler) {
        mytype.DEFMETHOD("to_mozilla_ast", function(parent) {
            return set_moz_loc(this, handler(this, parent));
        });
    }

    var TO_MOZ_STACK = null;

    function to_moz(node) {
        if (TO_MOZ_STACK === null) { TO_MOZ_STACK = []; }
        TO_MOZ_STACK.push(node);
        var ast = node != null ? node.to_mozilla_ast(TO_MOZ_STACK[TO_MOZ_STACK.length - 2]) : null;
        TO_MOZ_STACK.pop();
        if (TO_MOZ_STACK.length === 0) { TO_MOZ_STACK = null; }
        return ast;
    }

    function to_moz_in_destructuring() {
        var i = TO_MOZ_STACK.length;
        while (i--) {
            if (TO_MOZ_STACK[i] instanceof AST_Destructuring) {
                return true;
            }
        }
        return false;
    }

    function to_moz_block(node) {
        return {
            type: "BlockStatement",
            body: node.body.map(to_moz)
        };
    }

    function to_moz_scope(type, node) {
        var body = node.body.map(to_moz);
        if (node.body[0] instanceof AST_SimpleStatement && node.body[0].body instanceof AST_String) {
            body.unshift(to_moz(new AST_EmptyStatement(node.body[0])));
        }
        return {
            type: type,
            body: body
        };
    }
})();
