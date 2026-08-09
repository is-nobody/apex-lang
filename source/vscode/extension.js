// `npx @vscode/vsce package` for build .vsix
const vscode = require('vscode');

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

function activate(context) {
    // run file command using a terminal
    const runFile = vscode.commands.registerCommand('apex.runFile', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'apex') {
            vscode.window.showErrorMessage('No Apex file is open');
            return;
        }

        const filePath = editor.document.uri.fsPath;
        await editor.document.save();

        let terminal = vscode.window.terminals.find(t => t.name === 'Apex');
        if (!terminal) {
            terminal = vscode.window.createTerminal('Apex');
        }
        
        terminal.show();
        terminal.sendText(`apex "${filePath}"`);
    });

    // hover provider for documentation and type hints
    const hover = vscode.languages.registerHoverProvider('apex', {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position);
            const fullRange = document.getWordRangeAtPosition(position, /[a-zA-Z0-9_.]+/);
            const word = document.getText(fullRange);
            const simpleWord = document.getText(range);

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

                ...libDocs
            };

            if (docs[word]) {
                return new vscode.Hover(docs[word]);
            }
            
            if (docs[simpleWord]) {
                return new vscode.Hover(docs[simpleWord]);
            }

            return null;
        }
    });

    // completion provider for keywords and library functions
    const completion = vscode.languages.registerCompletionItemProvider('apex', {
        provideCompletionItems() {
            const keywords = [
                'function', 'if', 'elif', 'else', 'for', 'break',
                'continue', 'return', 'import', 'and', 'or', 'not',
                'true', 'false', 'none'
            ];

            const libs = Object.entries(libDocs).map(([label, detail]) => ({ label, detail }));

            const items = [];

            keywords.forEach(kw => {
                items.push(new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword));
            });

            libs.forEach(lib => {
                const item = new vscode.CompletionItem(lib.label, vscode.CompletionItemKind.Module);
                item.detail = lib.detail;
                items.push(item);
            });

            const libFuncs = [
                'os.output',             'os.input',
                'os.wait',               'os.exit',
                'os.set_current_folder', 'os.access',
                'os.get_current_folder', 'os.items',
                'os.terminate_process',  'os.execute',
                'os.create_folder',      'os.delete',
                'os.create_file',        'os.size',
                'os.parentfolder',       'os.copy',
                'os.read',               'os.write',
                'os.append',             'os.exists',
                'os.isfile',             'os.isfolder',
                'os.rename',             'os.move',

                'sys.platform',   'sys.architecture',
                'sys.hostname',   'sys.user',
                'sys.homedir',    'sys.apex_version',
                'sys.executable', 'sys.environment',
                'sys.disksize',   'sys.tempdir',
                'sys.isterminal', 'sys.process_id',
                'sys.time',       'sys.date',

                'math.abs',     'math.floor',
                'math.ceil',    'math.round',
                'math.sqrt',    'math.exp',
                'math.log',     'math.sin',
                'math.cos',     'math.tan',
                'math.asin',    'math.acos',
                'math.atan',    'math.pi',
                'math.e',       'math.inf',
                'math.isnan',   'math.isinf',
                'math.trunc',   'math.pow',
                'math.atan2',   'math.radians',
                'math.degrees', 'math.hypot',
                'math.gcd',     'math.factorial',

                'string.isletter', 'string.isnumber',
                'string.length',   'string.lower',
                'string.upper',    'string.sub',
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

                'random.random',      'random.randint',
                'random.choice',      'random.shuffle',
                'random.sample',      'random.gauss',
                'random.seed',        'random.triangular',
                'random.expovariate', 'random.betavariate',

                'codecs.json_read',     'codecs.json_write',
                'codecs.csv_read',      'codecs.csv_write',
                'codecs.xml_read',      'codecs.xml_write',
                'codecs.base_write',    'codecs.base_read',
                'codecs.baseurl_write', 'codecs.baseurl_read',

                'regex.findall', 'regex.sub',
                'regex.split',   'regex.search',

                'crypto.md5',            'crypto.sha1',
                'crypto.sha256',         'crypto.sha512',
                'crypto.hmac_md5',       'crypto.hmac_sha1',
                'crypto.hmac_sha256',    'crypto.hmac_sha512',
                'crypto.pbkdf2_md5',     'crypto.pbkdf2_sha1',
                'crypto.pbkdf2_sha256',  'crypto.pbkdf2_sha512',
                'crypto.aes128_encrypt', 'crypto.aes128_encrypt',
                'crypto.token_hex',      'crypto.secure_randint',
                'crypto.compare_digest',

                'number', 'string', 'type'
            ];

            libFuncs.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                items.push(item);
            });

            return items;
        }
    });

    // document symbols for outline view
    const symbols = vscode.languages.registerDocumentSymbolProvider('apex', {
        provideDocumentSymbols(document) {
            const result = [];
            const text = document.getText();

            const funcRegex = /function\s+([a-zA-Z_][a-zA-Z0-9_]*)/g;
            let match;
            while ((match = funcRegex.exec(text)) !== null) {
                const pos = document.positionAt(match.index);
                result.push(new vscode.DocumentSymbol(
                    match[1],
                    'function',
                    vscode.SymbolKind.Function,
                    new vscode.Range(pos, pos.translate(0, match[0].length)),
                    new vscode.Range(pos, pos.translate(0, match[0].length))
                ));
            }

            return result;
        }
    });

    context.subscriptions.push(runFile, hover, completion, symbols);
}

function deactivate() {}

module.exports = { activate, deactivate };