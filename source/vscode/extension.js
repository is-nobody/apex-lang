// source/vscode/extension.js
// Implementation of VS Code extension for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

const vscode = require('vscode');  // import vscode extension api: `npx @vscode/vsce package` for build .vsix

// documentation strings for standard library modules
const libDocs = {
    'os': 'OS library.',
    'sys': 'System information library.',
    'math': 'Mathematics library.',
    'string': 'String library.',
    'table': 'Table library.',
    'ffi': 'Foreign Function Interface library.',
    'random': 'Random generation.',
    'codecs': 'Encoding/decoding library.',
    'regex': 'Regular expressions library.',
    'crypto': 'Cryptography library.'
};

// extension activation entry point
function activate(context) {
    // run current file using an integrated terminal
    const runFile = vscode.commands.registerCommand('apex.runFile', async () => {
        const editor = vscode.window.activeTextEditor;                // get active editor
        if (!editor || editor.document.languageId !== 'apex') {       // check if apex file is open
            vscode.window.showErrorMessage('No Apex file is open');   // show error message
            return;
        }

        const filePath = editor.document.uri.fsPath;          // get absolute file path
        await editor.document.save();                         // save document before running

        let terminal = vscode.window.terminals.find(t => t.name === 'Apex');   // find existing apex terminal
        if (!terminal) {                                      // terminal doesn't exist yet
            terminal = vscode.window.createTerminal('Apex');  // create new terminal named Apex
        }
        
        terminal.show();                                      // bring terminal to front
        terminal.sendText(`apex "${filePath}"`);              // execute apex interpreter on current file
    });

    // hover provider for documentation and type hints
    const hover = vscode.languages.registerHoverProvider('apex', {
        provideHover(document, position) {                            // called when user hovers over text
            const range = document.getWordRangeAtPosition(position);  // get simple word range at cursor
            const fullRange = document.getWordRangeAtPosition(position, /[a-zA-Z0-9_.]+/);  // get extended range with dots
            const word = document.getText(fullRange);                 // full word including dots (e.g., "os.output")
            const simpleWord = document.getText(range);               // simple word without dots

            // documentation dictionary for keywords and language constructs
            const docs = {
                'function': 'Declares a function.\n\n```apex\nfunction name(params)\n    // code\n    return value\n```',
                'if': 'Conditional statement.\n\n```apex\nif condition\n    // code\nelif other_condition\n    // code\nelse\n    // code\n```',
                'elif': 'Else-if branch in conditional statements.',
                'else': 'Default branch in conditional statements.',
                'for': 'Numeric, table, and conditional loops.\n\n```apex\n// Numeric loop\nfor i = 1, 10\nfor i = 10, 1, -1\n\n// Table loop\nfor k = my_table\n\n// Conditional loop\nfor x < 10\n```',
                'return': 'Returns a value from a function.',
                'break': 'Exits the current loop immediately.',
                'continue': 'Skips the rest of the current loop iteration.',
                'import': 'Imports a file or library.\n\n```apex\nimport os\nimport utils.math\n```',
                'and': 'Logical AND operator.',
                'or': 'Logical OR operator.',
                'not': 'Logical NOT operator.',

                'none': 'Represents the absence of a value.',
                'true': 'Boolean literal — true.',
                'false': 'Boolean literal — false.',
                'number': 'Converts a value to a number.\n\n```apex\nnumber("42")  // 42\n```',
                'string': 'Converts a value to a string.\n\n```apex\nstring(42)  // "42"\n```',
                'type': 'Returns the type name of a value as a string.\n\n```apex\ntype(10)  // "number"\n```',

                ...libDocs                                  // merge library documentation
            };

            if (docs[word]) {                               // check if full word has documentation
                return new vscode.Hover(docs[word]);        // return hover for full word
            }
            
            if (docs[simpleWord]) {                         // check if simple word has documentation
                return new vscode.Hover(docs[simpleWord]);  // return hover for simple word
            }

            return null;                                    // no documentation available
        }
    });

    const completion = vscode.languages.registerCompletionItemProvider('apex', {
        provideCompletionItems() {         // called when user triggers autocomplete
            const keywords = [             // apex language keywords
                'function', 'if', 'elif', 'else', 'for', 'break',
                'continue', 'return', 'import', 'and', 'or', 'not',
                'true', 'false', 'none'
            ];

            const libs = Object.entries(libDocs).map(([label, detail]) => ({ label, detail }));  // convert lib docs to completion items

            const items = [];              // array to hold completion items

            keywords.forEach(kw => {       // add each keyword as completion item
                items.push(new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword));
            });

            libs.forEach(lib => {          // add each library as module completion item
                const item = new vscode.CompletionItem(lib.label, vscode.CompletionItemKind.Module);
                item.detail = lib.detail;  // attach library description
                items.push(item);
            });

            // list of standard library functions
            const libFuncs = [
                'os.output',             'os.input',
                'os.wait',               'os.exit',
                'os.change_folder', 'os.access',
                'os.current_folder', 'os.list_folder',
                'os.terminate',  'os.execute',
                'os.create_folder',      'os.delete',
                'os.create_file',        'os.size',
                'os.parent_folder',       'os.copy',
                'os.read',               'os.write',
                'os.append',             'os.exists',
                'os.is_file',             'os.is_folder',
                'os.rename',             'os.move',

                'sys.platform',   'sys.architecture',
                'sys.host',   'sys.user',
                'sys.home',    'sys.apex_version',
                'sys.executable', 'sys.environment',
                'sys.disk',   'sys.temp',
                'sys.is_terminal', 'sys.process_id',
                'sys.time',       'sys.datetime',

                'math.abs',     'math.round_down',
                'math.round_up',    'math.round',
                'math.sqrt',    'math.exponent',
                'math.log',     'math.sin',
                'math.cos',     'math.tan',
                'math.asin',    'math.acos',
                'math.atan',    'math.pi',
                'math.e',       'math.inf',
                'math.is_nan',   'math.is_inf',
                'math.drop_decimal',   'math.power',
                'math.atan2',   'math.radians',
                'math.degrees', 'math.hypotenuse',
                'math.gcd',     'math.factorial',

                'string.is_letter', 'string.is_number',
                'string.length',   'string.lower',
                'string.upper',    'string.slice',
                'string.split',    'string.join',
                'string.trim',     'string.find',
                'string.replace',

                'table.remove', 'table.has',
                'table.size',   'table.keys',
                'table.values', 'table.clear',
                'table.copy',   'table.merge',

                'ffi.open',   'ffi.call',
                'ffi.errno',  'ffi.strerror',
                'ffi.malloc', 'ffi.free',

                'random.float',      'random.integer',
                'random.choice',      'random.shuffle',
                'random.sample',      'random.gauss',
                'random.seed',        'random.triangular',
                'random.expovariate', 'random.betavariate',

                'codecs.json_decode',     'codecs.json_encode',
                'codecs.csv_decode',      'codecs.csv_encode',
                'codecs.xml_decode',      'codecs.xml_encode',
                'codecs.base64_encode',    'codecs.base64_decode',
                'codecs.base64url_encode', 'codecs.base64url_decode',
                'codecs.hex_encode',     'codecs.hex_decode',

                'regex.find_all', 'regex.replace',
                'regex.split',   'regex.search',

                'crypto.md5',            'crypto.sha1',
                'crypto.sha256',         'crypto.sha384',
                'crypto.sha512',
                'crypto.hmac_md5',       'crypto.hmac_sha1',
                'crypto.hmac_sha256',    'crypto.hmac_sha384',
                'crypto.hmac_sha512',
                'crypto.pbkdf2_md5',     'crypto.pbkdf2_sha1',
                'crypto.pbkdf2_sha256',  'crypto.pbkdf2_sha384',
                'crypto.pbkdf2_sha512',
                'crypto.aes128_encrypt', 'crypto.aes128_decrypt',
                'crypto.aes192_encrypt', 'crypto.aes192_decrypt',
                'crypto.aes256_encrypt', 'crypto.aes256_decrypt',
                'crypto.random_hex',      'crypto.random_integer',
                'crypto.compare_strings',

                'number', 'string', 'type'
            ];

            libFuncs.forEach(func => {  // add each library function as completion item
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                items.push(item);
            });

            return items;  // return all completion items
        }
    });

    // document symbols for outline view
    const symbols = vscode.languages.registerDocumentSymbolProvider('apex', {
        provideDocumentSymbols(document) {                     // called to build outline view
            const result = [];                                 // array to hold symbol items
            const text = document.getText();                   // get entire document text

            const funcRegex = /function\s+([a-zA-Z_][a-zA-Z0-9_]*)/g;  // regex to find function definitions
            let match;                                         // match result variable
            while ((match = funcRegex.exec(text)) !== null) {  // iterate over all function matches
                const pos = document.positionAt(match.index);  // get position of match
                result.push(new vscode.DocumentSymbol(         // create symbol for outline
                    match[1],                                  // function name
                    'function',                                // symbol kind description
                    vscode.SymbolKind.Function,                // symbol type
                    new vscode.Range(pos, pos.translate(0, match[0].length)),  // selection range
                    new vscode.Range(pos, pos.translate(0, match[0].length))   // full range
                ));
            }

            return result;  // return symbols for outline view
        }
    });

    context.subscriptions.push(runFile, hover, completion, symbols);  // register all providers
}

function deactivate() {}                    // extension deactivation (no cleanup needed)

module.exports = { activate, deactivate };  // export extension entry points