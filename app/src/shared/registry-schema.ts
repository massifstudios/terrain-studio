import type {
	NodeSchema,
	ParameterSchema,
	ParameterValue,
	PortTypeName,
	RegistrySchema
} from './graph-document';

/// Everything the UI knows about a node comes from here and nowhere else.

export function findNodeSchema(
	registry: RegistrySchema | null,
	typeName: string
): NodeSchema | null {
	if (!registry) {
		return null;
	}
	const found = registry.nodes.find((node) => node.typeName === typeName);
	if (!found) {
		return null;
	}
	return found;
}

/// Whether an edge carrying this type has any input on the node to land on.
export function acceptsInputType(schema: NodeSchema, type: PortTypeName): boolean {
	return schema.inputs.some((input) => input.type === type);
}

/// Which half of a parameter carries its value comes from the descriptor, the
/// same split the engine makes between `number` and `text`.
export const textualTypes: ReadonlySet<ParameterSchema['type']> = new Set([
	'path',
	'text',
	'colorRamp',
	'curve'
]);

export function defaultParameterValue(parameter: ParameterSchema): ParameterValue {
	if (parameter.type === 'bool') {
		return parameter.defaultValue !== 0;
	}
	if (textualTypes.has(parameter.type)) {
		return parameter.defaultText;
	}
	return parameter.defaultValue;
}

/// Every parameter is written out, so a later change to a default cannot
/// silently alter a saved project.
export function defaultParameters(schema: NodeSchema): Record<string, ParameterValue> {
	const parameters: Record<string, ParameterValue> = {};
	for (const parameter of schema.parameters) {
		parameters[parameter.name] = defaultParameterValue(parameter);
	}
	return parameters;
}

export interface NodeCategory {
	name: string;
	nodes: NodeSchema[];
}

export function groupByCategory(registry: RegistrySchema | null): NodeCategory[] {
	if (!registry) {
		return [];
	}
	const categories = new Map<string, NodeSchema[]>();
	for (const node of registry.nodes) {
		const existing = categories.get(node.category);
		if (existing) {
			existing.push(node);
			continue;
		}
		categories.set(node.category, [node]);
	}
	return [...categories.entries()]
		.map(([name, nodes]) => ({ name, nodes }))
		.sort((left, right) => left.name.localeCompare(right.name));
}
