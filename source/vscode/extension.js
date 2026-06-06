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
            const word = document.getText(range);

            const docs = {
                'function': 'Declares a function.\n\n```apex\nfunction name(params)\n    // code\n    return value\n```',
                'if': 'Conditional statement.\n\n```apex\nif condition\n    // code\nelif other_condition\n    // code\nelse\n    // code\n```',
                'for': 'For loop (inclusive end value).\n\n```apex\nfor i = 1, 10\n    // code\n\nfor i = 10, 1, -1\n    // code\n```',
                'while': 'While loop.\n\n```apex\nwhile condition\n    // code\n```',
                'return': 'Returns a value from a function.',
                'break': 'Exits the current loop immediately.',
                'continue': 'Skips the rest of the current loop iteration.',
                'import': 'Imports a file or library.\n\n```apex\nimport os\nimport utils.math\n```',
                'true': 'Boolean literal — true.',
                'false': 'Boolean literal — false.',
                'and': 'Logical AND operator.',
                'or': 'Logical OR operator.',
                'not': 'Logical NOT operator.',
                'elif': 'Else-if branch in conditional statements.',
                'number': 'Converts a value to a number.\n\n```apex\nnumber("42")  // 42\n```',
                'string': 'Converts a value to a string.\n\n```apex\nstring(42)  // "42"\n```',
                'os': 'OS library — system interaction.\n\nFunctions: output, input, read, write, exists, mkdir, etc.',
                'math': 'Math library — mathematical functions.\n\nFunctions: abs, floor, ceil, round, sqrt, exp, log, sin, cos, tan, etc.',
                'table': 'Table library — table manipulation.\n\nFunctions: remove, has, size, keys, values, clear, copy, merge.'
            };

            if (docs[word]) {
                return new vscode.Hover(docs[word]);
            }
            return null;
        }
    });

    // Completion provider
    const completion = vscode.languages.registerCompletionItemProvider('apex', {
        provideCompletionItems() {
            const keywords = [
                'function', 'if', 'elif', 'else', 'for', 'while',
                'break', 'continue', 'return', 'import', 'and', 'or', 'not',
                'true', 'false'
            ];

            const libs = [
                { label: 'os', detail: 'OS Library' },
                { label: 'math', detail: 'Math Library' },
                { label: 'string', detail: 'String Library' },
                { label: 'table', detail: 'Table Library' }
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

            // Library functions
            const libFuncs = [
                'os.output', 'os.input', 'os.read', 'os.write', 'os.exists',
                'os.mkdir', 'os.rmdir', 'os.listdir', 'os.time', 'os.wait',
                'os.system', 'os.platform', 'os.exit',
                'math.abs', 'math.floor', 'math.ceil', 'math.round', 'math.sqrt',
                'math.exp', 'math.log', 'math.sin', 'math.cos', 'math.tan',
                'string.len', 'string.lower', 'string.upper', 'string.sub',
                'string.split', 'string.join', 'string.trim', 'string.find', 'string.replace',
                'table.remove', 'table.has', 'table.size', 'table.keys',
                'table.values', 'table.clear', 'table.copy', 'table.merge'
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