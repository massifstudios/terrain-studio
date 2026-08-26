/**
 * The graph as text. Grammar, in full:
 *
 *     program   = { statement }
 *     statement = name "=" typeName "(" [ arguments ] ")"
 *     arguments = argument { "," argument }
 *     argument  = input | parameter
 *     input     = name [ "." outputPort ] | "_"
 *     parameter = name ":" value
 *     value     = number | string | "true" | "false" | enumOptionName
 *
 * A node must be defined before it is referred to, which is what makes a cycle
 * unwritable rather than merely rejected. Inputs are positional and come before
 * named parameters; `_` leaves one unconnected. An input without a port names
 * the first output, so `eroded` and `eroded.Out` mean the same thing.
 *
 * Nothing here knows a node type. Every type name, port name, parameter name
 * and parameter type is resolved against the registry the engine reports.
 */

import type {
	GraphEdgeDocument,
	GraphNodeDocument,
	NodeSchema,
	ParameterSchema,
	ParameterValue,
	PortSchema,
	RegistrySchema
} from './graph-document';
import { inDependencyOrder, type GraphContents } from './graph-operations';
import {
	defaultParameterValue,
	defaultParameters,
	findNodeSchema,
	textualTypes
} from './registry-schema';

/// Stands in an input position for a port that is deliberately left open.
export const unconnectedInput = '_';

export class DslError extends Error {
	readonly line: number;
	readonly column: number;

	constructor(message: string, line: number, column: number) {
		super(`Line ${line}: ${message}`);
		this.line = line;
		this.column = column;
	}
}

/* ------------------------------------------------------------- tokenizer -- */

type TokenKind = 'identifier' | 'number' | 'string' | 'symbol';

interface Token {
	kind: TokenKind;
	/// The identifier, the symbol, or the literal exactly as it was written.
	text: string;
	/// Meaningful for numbers and strings; the text itself otherwise.
	value: ParameterValue;
	line: number;
	column: number;
}

const identifierStart = /[A-Za-z_]/;
const identifierPart = /[A-Za-z0-9_]/;
const numberStart = /[-0-9]/;
const numberPart = /[0-9._eE+-]/;
const symbols = new Set(['(', ')', ',', ':', '.', '=']);

const stringEscapes: Record<string, string> = {
	'"': '"',
	'\\': '\\',
	n: '\n',
	t: '\t'
};

function isWhitespace(character: string): boolean {
	return character === ' ' || character === '\t' || character === '\n' || character === '\r';
}

class Scanner {
	#text: string;
	#index = 0;
	#line = 1;
	#column = 1;

	constructor(text: string) {
		this.#text = text;
	}

	atEnd(): boolean {
		return this.#index >= this.#text.length;
	}

	get line(): number {
		return this.#line;
	}

	get column(): number {
		return this.#column;
	}

	/// Whitespace and `#` comments carry no meaning and never reach the parser.
	skipIgnorable(): void {
		while (!this.atEnd()) {
			const character = this.#peek();
			if (character === '#') {
				this.#skipComment();
				continue;
			}
			if (!isWhitespace(character)) {
				return;
			}
			this.#advance();
		}
	}

	nextToken(): Token {
		const line = this.#line;
		const column = this.#column;
		const character = this.#peek();
		if (identifierStart.test(character)) {
			return this.#identifierToken(line, column);
		}
		if (numberStart.test(character)) {
			return this.#numberToken(line, column);
		}
		if (character === '"') {
			return this.#stringToken(line, column);
		}
		if (symbols.has(character)) {
			this.#advance();
			return { kind: 'symbol', text: character, value: character, line, column };
		}
		throw new DslError(`Unexpected character '${character}'`, line, column);
	}

	#peek(): string {
		return this.#text[this.#index];
	}

	#advance(): string {
		const character = this.#text[this.#index];
		this.#index += 1;
		this.#column += 1;
		if (character === '\n') {
			this.#line += 1;
			this.#column = 1;
		}
		return character;
	}

	#skipComment(): void {
		while (!this.atEnd() && this.#peek() !== '\n') {
			this.#advance();
		}
	}

	#identifierToken(line: number, column: number): Token {
		let text = '';
		while (!this.atEnd() && identifierPart.test(this.#peek())) {
			text += this.#advance();
		}
		return { kind: 'identifier', text, value: text, line, column };
	}

	/// Digit separators are read and then forgotten: `2_000_000` is a number,
	/// never a spelling the printer has to reproduce.
	#numberToken(line: number, column: number): Token {
		let text = this.#advance();
		while (!this.atEnd() && numberPart.test(this.#peek())) {
			text += this.#advance();
		}
		const value = Number(text.replaceAll('_', ''));
		if (!Number.isFinite(value)) {
			throw new DslError(`'${text}' is not a number`, line, column);
		}
		return { kind: 'number', text, value, line, column };
	}

	#stringToken(line: number, column: number): Token {
		this.#advance();
		let value = '';
		while (!this.atEnd() && this.#peek() !== '"') {
			value += this.#stringCharacter(line, column);
		}
		if (this.atEnd()) {
			throw new DslError('Unterminated text', line, column);
		}
		this.#advance();
		return { kind: 'string', text: value, value, line, column };
	}

	#stringCharacter(line: number, column: number): string {
		const character = this.#advance();
		if (character !== '\\') {
			return character;
		}
		const escaped = stringEscapes[this.#advance()];
		if (escaped === undefined) {
			throw new DslError('Unknown escape in text', line, column);
		}
		return escaped;
	}
}

function tokenize(text: string): Token[] {
	const tokens: Token[] = [];
	const scanner = new Scanner(text);
	while (true) {
		scanner.skipIgnorable();
		if (scanner.atEnd()) {
			return tokens;
		}
		tokens.push(scanner.nextToken());
	}
}

/* ---------------------------------------------------------------- values -- */

function readBoolean(token: Token): boolean {
	if (token.text === 'true') {
		return true;
	}
	if (token.text === 'false') {
		return false;
	}
	throw new DslError('Expected true or false', token.line, token.column);
}

function readNumber(token: Token): number {
	if (token.kind !== 'number') {
		throw new DslError('Expected a number', token.line, token.column);
	}
	return token.value as number;
}

function readString(token: Token): string {
	if (token.kind !== 'string') {
		throw new DslError('Expected quoted text', token.line, token.column);
	}
	return token.value as string;
}

/// An enum reads as the option's name, and as its raw number for anything the
/// registry has since renamed.
function readEnum(parameter: ParameterSchema, token: Token): number {
	if (token.kind === 'number') {
		return token.value as number;
	}
	const option = parameter.options.find((entry) => entry.name === token.text);
	if (!option) {
		const names = parameter.options.map((entry) => entry.name).join(', ');
		throw new DslError(`'${token.text}' is not one of ${names}`, token.line, token.column);
	}
	return option.value;
}

/* ---------------------------------------------------------------- parser -- */

interface OutputReference {
	nodeId: string;
	port: number;
	schema: NodeSchema;
}

class Parser {
	#tokens: Token[];
	#registry: RegistrySchema;
	#index = 0;
	#nodes: GraphNodeDocument[] = [];
	#edges: GraphEdgeDocument[] = [];

	constructor(tokens: Token[], registry: RegistrySchema) {
		this.#tokens = tokens;
		this.#registry = registry;
	}

	parse(): GraphContents {
		while (this.#index < this.#tokens.length) {
			this.#statement();
		}
		return { nodes: this.#nodes, edges: this.#edges };
	}

	#statement(): void {
		const name = this.#expectIdentifier('a node name');
		this.#expectSymbol('=');
		const typeToken = this.#expectIdentifier('a node type');
		const schema = this.#schemaOf(typeToken);
		if (this.#findNode(name.text)) {
			throw new DslError(`'${name.text}' is defined twice`, name.line, name.column);
		}

		const node: GraphNodeDocument = {
			id: name.text,
			typeName: schema.typeName,
			positionX: 0,
			positionY: 0,
			parameters: defaultParameters(schema)
		};
		this.#expectSymbol('(');
		this.#arguments(node, schema);
		this.#nodes.push(node);
	}

	#schemaOf(typeToken: Token): NodeSchema {
		const schema = findNodeSchema(this.#registry, typeToken.text);
		if (!schema) {
			throw new DslError(
				`Unknown node type '${typeToken.text}' — is a plugin missing?`,
				typeToken.line,
				typeToken.column
			);
		}
		return schema;
	}

	#arguments(node: GraphNodeDocument, schema: NodeSchema): void {
		let inputIndex = 0;
		let seenParameter = false;
		while (!this.#takeSymbol(')')) {
			const consumed = this.#argument(node, schema, inputIndex, seenParameter);
			inputIndex += consumed;
			seenParameter = seenParameter || consumed === 0;
			this.#separator();
		}
	}

	/// How many input ports the argument used up: one for an input, none for a
	/// named parameter.
	#argument(
		node: GraphNodeDocument,
		schema: NodeSchema,
		inputIndex: number,
		seenParameter: boolean
	): number {
		if (this.#looksLikeParameter()) {
			this.#parameter(node, schema);
			return 0;
		}
		if (seenParameter) {
			const token = this.#peek('an argument');
			throw new DslError('Inputs come before named parameters', token.line, token.column);
		}
		this.#input(node, schema, inputIndex);
		return 1;
	}

	#separator(): void {
		if (this.#atSymbol(')')) {
			return;
		}
		this.#expectSymbol(',');
	}

	#input(target: GraphNodeDocument, schema: NodeSchema, inputIndex: number): void {
		const token = this.#expectIdentifier('an input');
		const port = schema.inputs[inputIndex];
		if (!port) {
			throw new DslError(
				`${schema.typeName} takes ${schema.inputs.length} inputs`,
				token.line,
				token.column
			);
		}
		if (token.text === unconnectedInput) {
			return;
		}

		const source = this.#resolveOutput(token);
		this.#checkPortTypes(port, source, token);
		this.#edges.push({
			fromNodeId: source.nodeId,
			fromPort: source.port,
			toNodeId: target.id,
			toPort: inputIndex
		});
	}

	#resolveOutput(token: Token): OutputReference {
		const source = this.#findNode(token.text);
		if (!source) {
			throw new DslError(`Unknown node '${token.text}'`, token.line, token.column);
		}
		const schema = findNodeSchema(this.#registry, source.typeName) as NodeSchema;
		if (!this.#takeSymbol('.')) {
			return { nodeId: source.id, port: 0, schema };
		}
		return { nodeId: source.id, port: this.#outputPort(schema), schema };
	}

	#outputPort(schema: NodeSchema): number {
		const token = this.#next('an output name');
		if (token.kind === 'number') {
			return this.#checkedPort(schema, token.value as number, token);
		}
		const index = schema.outputs.findIndex((port) => port.name === token.text);
		if (index < 0) {
			throw new DslError(
				`${schema.typeName} has no output '${token.text}'`,
				token.line,
				token.column
			);
		}
		return index;
	}

	#checkedPort(schema: NodeSchema, port: number, token: Token): number {
		if (port < 0 || port >= schema.outputs.length) {
			throw new DslError(
				`${schema.typeName} has no output ${port}`,
				token.line,
				token.column
			);
		}
		return port;
	}

	#checkPortTypes(port: PortSchema, source: OutputReference, token: Token): void {
		const output = source.schema.outputs[source.port];
		if (output.type === port.type) {
			return;
		}
		throw new DslError(
			`${source.nodeId} produces ${output.type}, but this input takes ${port.type}`,
			token.line,
			token.column
		);
	}

	#parameter(node: GraphNodeDocument, schema: NodeSchema): void {
		const name = this.#expectIdentifier('a parameter name');
		this.#expectSymbol(':');
		const parameter = schema.parameters.find((entry) => entry.name === name.text);
		if (!parameter) {
			throw new DslError(
				`${schema.typeName} has no parameter '${name.text}'`,
				name.line,
				name.column
			);
		}
		node.parameters[parameter.name] = this.#value(parameter);
	}

	#value(parameter: ParameterSchema): ParameterValue {
		const token = this.#next('a value');
		if (parameter.type === 'bool') {
			return readBoolean(token);
		}
		if (textualTypes.has(parameter.type)) {
			return readString(token);
		}
		if (parameter.type === 'enum') {
			return readEnum(parameter, token);
		}
		return readNumber(token);
	}

	#findNode(id: string): GraphNodeDocument | null {
		const found = this.#nodes.find((node) => node.id === id);
		if (!found) {
			return null;
		}
		return found;
	}

	/* ------------------------------------------------------ token cursor -- */

	#next(what: string): Token {
		const token = this.#peek(what);
		this.#index += 1;
		return token;
	}

	#peek(what: string): Token {
		const token = this.#tokens[this.#index];
		if (!token) {
			const last = this.#tokens[this.#tokens.length - 1];
			throw new DslError(`Expected ${what}`, lineOf(last), columnOf(last));
		}
		return token;
	}

	#atSymbol(text: string): boolean {
		const token = this.#tokens[this.#index];
		if (!token) {
			return false;
		}
		return token.kind === 'symbol' && token.text === text;
	}

	#takeSymbol(text: string): boolean {
		if (!this.#atSymbol(text)) {
			return false;
		}
		this.#index += 1;
		return true;
	}

	#expectSymbol(text: string): Token {
		const token = this.#next(`'${text}'`);
		if (token.kind !== 'symbol' || token.text !== text) {
			throw new DslError(`Expected '${text}'`, token.line, token.column);
		}
		return token;
	}

	#expectIdentifier(what: string): Token {
		const token = this.#next(what);
		if (token.kind !== 'identifier') {
			throw new DslError(`Expected ${what}`, token.line, token.column);
		}
		return token;
	}

	#looksLikeParameter(): boolean {
		const name = this.#tokens[this.#index];
		const colon = this.#tokens[this.#index + 1];
		if (!name || !colon) {
			return false;
		}
		return name.kind === 'identifier' && colon.kind === 'symbol' && colon.text === ':';
	}
}

function lineOf(token: Token | undefined): number {
	if (!token) {
		return 1;
	}
	return token.line;
}

function columnOf(token: Token | undefined): number {
	if (!token) {
		return 1;
	}
	return token.column;
}

export function parseDsl(text: string, registry: RegistrySchema): GraphContents {
	return new Parser(tokenize(text), registry).parse();
}

/* --------------------------------------------------------------- printer -- */

function quoteText(value: string): string {
	const escaped = value.replaceAll('\\', '\\\\').replaceAll('"', '\\"');
	return `"${escaped.replaceAll('\n', '\\n').replaceAll('\t', '\\t')}"`;
}

function printEnum(parameter: ParameterSchema, value: number): string {
	const option = parameter.options.find((entry) => entry.value === value);
	if (!option) {
		return String(value);
	}
	return option.name;
}

function printLiteral(value: ParameterValue): string {
	if (typeof value === 'string') {
		return quoteText(value);
	}
	return String(value);
}

function printValue(parameter: ParameterSchema, value: ParameterValue): string {
	if (parameter.type === 'enum' && typeof value === 'number') {
		return printEnum(parameter, value);
	}
	return printLiteral(value);
}

/// Only what was changed is written; a parameter left at its default is noise,
/// and parsing puts it back from the registry.
function printedParameters(node: GraphNodeDocument, schema: NodeSchema): string[] {
	const written: string[] = [];
	for (const parameter of schema.parameters) {
		const value = node.parameters[parameter.name];
		if (value === undefined || value === defaultParameterValue(parameter)) {
			continue;
		}
		written.push(`${parameter.name}: ${printValue(parameter, value)}`);
	}
	return written;
}

function outputReference(edge: GraphEdgeDocument, sourceSchema: NodeSchema | null): string {
	if (edge.fromPort === 0) {
		return edge.fromNodeId;
	}
	const port = sourceSchema?.outputs[edge.fromPort];
	if (!port) {
		return `${edge.fromNodeId}.${edge.fromPort}`;
	}
	return `${edge.fromNodeId}.${port.name}`;
}

function trimTrailing(inputs: string[]): string[] {
	const kept = [...inputs];
	while (kept.length > 0 && kept[kept.length - 1] === unconnectedInput) {
		kept.pop();
	}
	return kept;
}

function printedInputs(
	node: GraphNodeDocument,
	schema: NodeSchema,
	contents: GraphContents,
	registry: RegistrySchema
): string[] {
	const written: string[] = [];
	for (let port = 0; port < schema.inputs.length; port += 1) {
		const edge = contents.edges.find(
			(entry) => entry.toNodeId === node.id && entry.toPort === port
		);
		written.push(referenceOrGap(edge, contents, registry));
	}
	return trimTrailing(written);
}

function referenceOrGap(
	edge: GraphEdgeDocument | undefined,
	contents: GraphContents,
	registry: RegistrySchema
): string {
	if (!edge) {
		return unconnectedInput;
	}
	const source = contents.nodes.find((entry) => entry.id === edge.fromNodeId);
	if (!source) {
		return unconnectedInput;
	}
	return outputReference(edge, findNodeSchema(registry, source.typeName));
}

/// A node whose type the registry does not know — a project saved against a
/// plugin that is not loaded. It still prints everything it holds, so the code
/// view never shows a node emptier than the graph really is.
function placeholderArguments(
	node: GraphNodeDocument,
	contents: GraphContents,
	registry: RegistrySchema
): string {
	const incoming = contents.edges.filter((edge) => edge.toNodeId === node.id);
	const inputs: string[] = [];
	for (let port = 0; port <= Math.max(-1, ...incoming.map((edge) => edge.toPort)); port += 1) {
		inputs.push(referenceOrGap(incoming.find((edge) => edge.toPort === port), contents, registry));
	}
	const parameters = Object.keys(node.parameters)
		.sort()
		.map((name) => `${name}: ${printLiteral(node.parameters[name])}`);
	return [...inputs, ...parameters].join(', ');
}

function argumentsOf(
	node: GraphNodeDocument,
	contents: GraphContents,
	registry: RegistrySchema
): string {
	const schema = findNodeSchema(registry, node.typeName);
	if (!schema) {
		return placeholderArguments(node, contents, registry);
	}
	const inputs = printedInputs(node, schema, contents, registry);
	return [...inputs, ...printedParameters(node, schema)].join(', ');
}

export function printDsl(contents: GraphContents, registry: RegistrySchema): string {
	const ordered = inDependencyOrder(contents);
	if (ordered.length === 0) {
		return '';
	}
	const width = Math.max(...ordered.map((node) => node.id.length));
	const lines = ordered.map((node) => {
		const name = node.id.padEnd(width);
		return `${name} = ${node.typeName}(${argumentsOf(node, contents, registry)})`;
	});
	return `${lines.join('\n')}\n`;
}
