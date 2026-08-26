/**
 * Discovering what the engine can do. Nothing here names a node type: every
 * line comes from the registry, so a plugin's node lists and describes exactly
 * like a built-in one.
 */

import type { NodeSchema, ParameterSchema, PortSchema, RegistrySchema } from '$shared/graph-document';
import { defaultParameterValue, groupByCategory } from '$shared/registry-schema';

/// The DSL call the node is written as, so the listing doubles as a reminder of
/// the order its inputs go in.
function signature(schema: NodeSchema): string {
	const inputs = schema.inputs.map((port) => port.name).join(', ');
	const outputs = schema.outputs.map((port) => port.name).join(', ');
	return `${schema.typeName}(${inputs}) -> ${outputs}`;
}

function parameterNames(schema: NodeSchema): string {
	return schema.parameters.map((parameter) => parameter.name).join(', ');
}

export function listNodes(registry: RegistrySchema): void {
	for (const category of groupByCategory(registry)) {
		console.log(category.name);
		for (const schema of category.nodes) {
			console.log(`  ${signature(schema)}`);
			if (schema.parameters.length > 0) {
				console.log(`      ${parameterNames(schema)}`);
			}
		}
	}
}

function portLine(port: PortSchema): string {
	const kind = describePortType(port);
	if (port.optional) {
		return `  ${port.name.padEnd(12)} ${kind.padEnd(22)} optional`;
	}
	return `  ${port.name.padEnd(12)} ${kind}`;
}

function describePortType(port: PortSchema): string {
	if (port.type !== 'field') {
		return port.type;
	}
	return `field/${port.fieldSemantics}`;
}

function describeRange(parameter: ParameterSchema): string {
	if (parameter.type === 'enum') {
		return `one of: ${parameter.options.map((option) => option.name).join(', ')}`;
	}
	if (parameter.minimum === parameter.maximum) {
		return '';
	}
	return `${parameter.minimum}..${parameter.maximum}`;
}

function parameterLine(parameter: ParameterSchema): string {
	const columns = [
		`  ${parameter.name.padEnd(20)}`,
		parameter.type.padEnd(10),
		`default ${String(defaultParameterValue(parameter))}`.padEnd(24),
		parameter.unit.padEnd(6),
		describeRange(parameter)
	];
	return columns.join(' ').trimEnd();
}

function printSection(title: string, lines: string[]): void {
	if (lines.length === 0) {
		return;
	}
	console.log(title);
	for (const line of lines) {
		console.log(line);
	}
}

export function describeNode(registry: RegistrySchema, typeName: string): void {
	const schema = registry.nodes.find((node) => node.typeName === typeName);
	if (!schema) {
		throw new Error(`No node type '${typeName}' — is a plugin missing?`);
	}
	console.log(`${schema.typeName}  (${schema.category}, version ${schema.version})`);
	console.log(schema.description);
	console.log('');
	printSection('Inputs', schema.inputs.map(portLine));
	printSection('Outputs', schema.outputs.map(portLine));
	printSection('Parameters', schema.parameters.map(parameterLine));
}
