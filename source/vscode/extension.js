// `npx @vscode/vsce package` for build .vsix
const vscode = require('vscode');

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

                'os': 'OS library — system interaction, file I/O, and process management.\n\nFunctions: output, input, time, wait, exit, get_current_folder, set_current_folder, terminate_process, execute, read, write, append, exists, isfile, isfolder, size, stat, filetype, create_file, create_folder, delete, rename, move, copy, items, parentfolder, access.',
                'sys': 'System information library.\n\nFunctions: platform, architecture, hostname, user, homedir, apex_version, executable, environment, disksize, tempdir, isterminal, process_id.',
                'math': 'Math library — mathematical functions.\n\nFunctions: abs, floor, ceil, round, sqrt, exp, log, sin, cos, tan, pi, e, inf, pow, gcd, factorial, isnan, isinf, trunc, atan2, radians, degrees, hypot.',
                'string': 'String manipulation library.\n\nFunctions: len, lower, upper, sub, split, join, trim, find, replace.',
                'table': 'Table (hash map/array) library.\n\nFunctions: remove, has, size, keys, values, clear, copy, merge.',
                'ffi': 'Foreign Function Interface.\n\nFunctions: open, call, errno, strerror, malloc, free.',
                'random': 'Random generation.\n\nFunctions: random, randint, choice, shuffle, sample, gauss, seed, triangular, expovariate, betavariate, secure_token_hex, secure_randint, compare_digest.',
                'codecs': 'Data encoding/decoding.\n\nFunctions: json_read, json_write, csv_read, csv_write, xml_read, xml_write, base_write, base_read, baseurl_write, baseurl_read.',
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

            const libs = [
                { label: 'os', detail: 'OS, File System & Process Library' },
                { label: 'sys', detail: 'System Info Library' },
                { label: 'math', detail: 'Math Library' },
                { label: 'string', detail: 'String Library' },
                { label: 'table', detail: 'Table Library' },
                { label: 'ffi', detail: 'Foreign Function Interface' },
                { label: 'random', detail: 'Random generation' },
                { label: 'codecs', detail: 'Data Encoding/Decoding' }
            ];

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
                'os.output', 'os.input',
                'os.wait', 'os.exit', 
                'os.get_current_folder', 'os.set_current_folder',
                'os.terminate_process', 'os.execute',
                'os.read', 'os.write', 'os.append',
                'os.exists', 'os.isfile', 'os.isfolder', 
                'os.size', 'os.stat', 'os.filetype',
                'os.create_file', 'os.create_folder', 
                'os.delete', 'os.rename', 'os.move', 'os.copy',
                'os.items', 'os.parentfolder', 'os.access',
                'sys.platform', 'sys.architecture', 'sys.hostname', 'sys.user',
                'sys.homedir', 'sys.apex_version', 'sys.executable', 
                'sys.environment', 'sys.disksize', 'sys.tempdir', 
                'sys.isterminal', 'sys.process_id', 'sys.time', 'sys.date',
                'math.abs', 'math.floor', 'math.ceil', 'math.round',
                'math.sqrt', 'math.exp', 'math.log',
                'math.sin', 'math.cos', 'math.tan',
                'math.asin', 'math.acos', 'math.atan',
                'math.pi', 'math.e', 'math.inf',
                'math.isnan', 'math.isinf', 'math.trunc',
                'math.pow', 'math.atan2',
                'math.radians', 'math.degrees', 'math.hypot',
                'math.gcd', 'math.factorial',
                'string.isletter', 'string.isnumber',
                'string.length', 'string.lower', 'string.upper',
                'string.sub', 'string.split', 'string.join',
                'string.trim', 'string.find', 'string.replace',
                'table.remove', 'table.has', 'table.size',
                'table.keys', 'table.values', 'table.clear',
                'table.copy', 'table.merge',
                'ffi.open', 'ffi.call', 'ffi.errno', 'ffi.strerror',
                'ffi.malloc', 'ffi.free',
                'random.random', 'random.randint', 'random.choice', 'random.shuffle',
                'random.sample', 'random.gauss', 'random.seed',
                'random.triangular', 'random.expovariate', 'random.betavariate',
                'random.secure_token_hex', 'random.secure_randint', 'random.compare_digest',
                'codecs.json_read', 'codecs.json_write',
                'codecs.csv_read', 'codecs.csv_write',
                'codecs.xml_read', 'codecs.xml_write',
                'codecs.base_write', 'codecs.base_read',
                'codecs.baseurl_write', 'codecs.baseurl_read',
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