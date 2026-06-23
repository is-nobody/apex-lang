// `npx @vscode/vsce package` for build .vsix
const vscode = require('vscode');

function activate(context) {
    console.log('Apex language support activated');
    
    // Run file command - uses terminal
    const runFile = vscode.commands.registerCommand('apex.runFile', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'apex') {
            vscode.window.showErrorMessage('No Apex file is open');
            return;
        }

        const filePath = editor.document.uri.fsPath;
        await editor.document.save();

        // Get or create a terminal
        let terminal = vscode.window.terminals.find(t => t.name === 'Apex');
        if (!terminal) {
            terminal = vscode.window.createTerminal('Apex');
        }
        
        terminal.show();
        terminal.sendText(`apex "${filePath}"`);
    });

    // Hover provider
    const hover = vscode.languages.registerHoverProvider('apex', {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position);
            // Handle dotted access (e.g., os.output) by getting the full word including dots
            const fullRange = document.getWordRangeAtPosition(position, /[a-zA-Z0-9_.]+/);
            const word = document.getText(fullRange);
            // Fallback to simple word if regex doesn't match or for keywords
            const simpleWord = document.getText(range);

            const docs = {
                // Keywords
                'function': 'Declares a function.\n\n```apex\nfunction name(params)\n    // code\n    return value\n```',
                'if': 'Conditional statement.\n\n```apex\nif condition\n    // code\nelif other_condition\n    // code\nelse\n    // code\n```',
                'elif': 'Else-if branch in conditional statements.',
                'else': 'Default branch in conditional statements.',
                'for': 'For loop (inclusive end value).\n\n```apex\nfor i = 1, 10\n    // code\n\nfor i = 10, 1, -1\n    // code\n\nfor k = my_table\n    // iterate keys\n```',
                'return': 'Returns a value from a function.',
                'break': 'Exits the current loop immediately.',
                'continue': 'Skips the rest of the current loop iteration.',
                'import': 'Imports a file or library.\n\n```apex\nimport os\nimport utils.math\n```',
                'and': 'Logical AND operator.',
                'or': 'Logical OR operator.',
                'not': 'Logical NOT operator.',
                
                // Literals & Types
                'true': 'Boolean literal — true.',
                'false': 'Boolean literal — false.',
                'number': 'Converts a value to a number.\n\n```apex\nnumber("42")  // 42\n```',
                'string': 'Converts a value to a string.\n\n```apex\nstring(42)  // "42"\n```',
                'type': 'Returns the type name of a value as a string.\n\n```apex\ntype(10)  // "number"\n```',

                // Modules
                'os': 'OS library — system interaction.\n\nFunctions: output, input, getenv, setenv, time, wait, exit, getcd, setcd, pid, spawn, execute, etc.',
                'files': 'Files library — file system operations.\n\nFunctions: read, write, append, exists, isfile, isfolder, filesize, create_file, delete_file, rename_file, copy_file, listfolders, etc.',
                'sys': 'System information library.\n\nProperties: platform, architecture, hostname, user, homedir, apex_version, executable, disksize, tempdir, isterminal.',
                'math': 'Math library — mathematical functions.\n\nFunctions: abs, floor, ceil, round, sqrt, exp, log, sin, cos, tan, pi, e, inf, pow, gcd, factorial, etc.',
                'string': 'String manipulation library.\n\nFunctions: len, lower, upper, sub, split, join, trim, find, replace.',
                'table': 'Table (hash map/array) library.\n\nFunctions: remove, has, size, keys, values, clear, copy, merge.',
                'ffi': 'Foreign Function Interface.\n\nFunctions: open, call, errno, strerror, malloc, free.',
                'random': 'Random generation.\n\nFunctions: random, randint, choice, shuffle, sample, choices, gauss, seed, triangular, expovariate, betavariate.',
                'codecs': 'Data encoding/decoding.\n\nFunctions: json_read, json_write, csv_read, csv_write, xml_read, xml_write, yaml_read, toml_read, base_write, base_read, baseurl_write, baseurl_read.',
            };

            // Check for specific module properties/functions first
            if (docs[word]) {
                return new vscode.Hover(docs[word]);
            }
            
            // Check for simple keywords if dotted lookup failed
            if (docs[simpleWord]) {
                return new vscode.Hover(docs[simpleWord]);
            }

            return null;
        }
    });

    // Completion provider
    const completion = vscode.languages.registerCompletionItemProvider('apex', {
        provideCompletionItems() {
            const keywords = [
                'function', 'if', 'elif', 'else', 'for', 'break',
                'continue', 'return', 'import', 'and', 'or', 'not',
                'true', 'false'
            ];

            const libs = [
                { label: 'os', detail: 'OS Library' },
                { label: 'files', detail: 'File System Library' },
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

            // Library functions (Comprehensive list from codegen.c)
            const libFuncs = [
                // os
                'os.output', 'os.input', 'os.getenv', 'os.setenv', 'os.env',
                'os.time', 'os.wait', 'os.exit', 'os.getcd', 'os.setcd',
                'os.pid', 'os.kill', 'os.execute',
                
                // files
                'files.read', 'files.write', 'files.append',
                'files.exists', 'files.isfile', 'files.isfolder', 'files.filesize', 'files.foldersize',
                'files.filetype', 'files.stat',
                'files.create_file', 'files.create_folder', 'files.delete_file', 'files.delete_folder',
                'files.rename_file', 'files.rename_folder', 'files.move_file', 'files.move_folder',
                'files.copy_file', 'files.copy_folder',
                'files.listfolders', 'files.parentfolder', 'files.access',

                // sys
                'sys.platform', 'sys.architecture', 'sys.hostname', 'sys.user',
                'sys.homedir', 'sys.apex_version', 'sys.executable',
                'sys.disksize', 'sys.tempdir', 'sys.isterminal',

                // math
                'math.abs', 'math.floor', 'math.ceil', 'math.round',
                'math.sqrt', 'math.exp', 'math.log',
                'math.sin', 'math.cos', 'math.tan',
                'math.asin', 'math.acos', 'math.atan',
                'math.pi', 'math.e', 'math.inf',
                'math.isnan', 'math.isinf', 'math.trunc',
                'math.pow', 'math.atan2',
                'math.radians', 'math.degrees', 'math.hypot',
                'math.gcd', 'math.factorial',

                // string
                'string.len', 'string.lower', 'string.upper',
                'string.sub', 'string.split', 'string.join',
                'string.trim', 'string.find', 'string.replace',

                // table
                'table.remove', 'table.has', 'table.size',
                'table.keys', 'table.values', 'table.clear',
                'table.copy', 'table.merge',

                // ffi
                'ffi.open', 'ffi.call', 'ffi.errno', 'ffi.strerror',
                'ffi.malloc', 'ffi.free',

                // random
                'random.random', 'random.randint', 'random.choice', 'random.shuffle',
                'random.sample', 'random.gauss', 'random.seed',
                'random.triangular', 'random.expovariate', 'random.betavariate',
                'random.secure_token_hex', 'random.secure_token_bytes',
                'random.secure_randint', 'random.compare_digest',

                // codecs
                'codecs.json_read', 'codecs.json_write',
                'codecs.csv_read', 'codecs.csv_write',
                'codecs.xml_read', 'codecs.xml_write',
                'codecs.yaml_read', 'codecs.toml_read',
                'codecs.base_write', 'codecs.base_read',
                'codecs.baseurl_write', 'codecs.baseurl_read'
            ];

            libFuncs.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                items.push(item);
            });

            return items;
        }
    });

    // Document symbols (outline)
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