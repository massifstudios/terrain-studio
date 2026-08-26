import { describe, expect, test } from 'bun:test';
import type {
	NodeSchema,
	ParameterSchema,
	PortSchema,
	RegistrySchema
} from './graph-document';
import { DslError, parseDsl, printDsl } from './graph-dsl';
import type { GraphContents } from './graph-operations';

function port(name: string, type: PortSchema['type'], optional = false): PortSchema {
	return { name, description: '', type, fieldSemantics: 'heightfield', optional };
}

function parameter(partial: Partial<ParameterSchema> & { name: string }): ParameterSchema {
	return {
		label: partial.name,
		description: '',
		unit: '',
		type: 'float',
		defaultValue: 0,
		minimum: 0,
		maximum: 100,
		defaultText: '',
		options: [],
		...partial
	};
}

function schema(
	typeName: string,
	inputs: PortSchema[],
	outputs: PortSchema[],
	parameters: ParameterSchema[] = []
): NodeSchema {
	return { typeName, category: 'Test', description: '', version: 1, inputs, outputs, parameters };
}

const registry: RegistrySchema = {
	abiVersion: 1,
	nodes: [
		schema(
			'ImportDEM',
			[],
			[port('Out', 'field')],
			[parameter({ name: 'path', type: 'path', defaultText: '' })]
		),
		schema(
			'Blur',
			[port('In', 'field'), port('Mask', 'field', true)],
			[port('Out', 'field')],
			[parameter({ name: 'radius', defaultValue: 50 })]
		),
		schema(
			'HydraulicErosion',
			[port('In', 'field'), port('Mask', 'field', true)],
			[port('Out', 'field'), port('Wear', 'field'), port('Flow', 'field')],
			[parameter({ name: 'capacity', defaultValue: 1 })]
		),
		schema(
			'ColorRamp',
			[port('In', 'field')],
			[port('Out', 'color')],
			[parameter({ name: 'ramp', type: 'colorRamp', defaultText: '0:#000000;1:#ffffff' })]
		),
		schema(
			'ExportPoints',
			[port('In', 'points')],
			[],
			[
				parameter({
					name: 'format',
					type: 'enum',
					defaultValue: 0,
					options: [
						{ name: 'Csv', label: 'CSV', value: 0 },
						{ name: 'Json', label: 'JSON', value: 1 }
					]
				}),
				parameter({ name: 'enabled', type: 'bool', defaultValue: 1 })
			]
		)
	]
};

function roundTrip(text: string): string {
	return printDsl(parseDsl(text, registry), registry);
}

function errorFrom(text: string): DslError {
	try {
		parseDsl(text, registry);
	} catch (error) {
		return error as DslError;
	}
	throw new Error('Expected the text to be rejected');
}

describe('printing', () => {
	test('lines up the names and leaves defaults out', () => {
		const contents: GraphContents = {
			nodes: [
				{
					id: 'dem',
					typeName: 'ImportDEM',
					positionX: 0,
					positionY: 0,
					parameters: { path: 'diffusion.massifdem' }
				},
				{
					id: 'smoothed',
					typeName: 'Blur',
					positionX: 0,
					positionY: 0,
					parameters: { radius: 50 }
				}
			],
			edges: [{ fromNodeId: 'dem', fromPort: 0, toNodeId: 'smoothed', toPort: 0 }]
		};

		expect(printDsl(contents, registry)).toBe(
			'dem      = ImportDEM(path: "diffusion.massifdem")\n' + 'smoothed = Blur(dem)\n'
		);
	});

	test('writes nodes in dependency order whatever order they are held in', () => {
		const contents: GraphContents = {
			nodes: [
				{ id: 'b', typeName: 'Blur', positionX: 0, positionY: 0, parameters: {} },
				{ id: 'a', typeName: 'ImportDEM', positionX: 0, positionY: 0, parameters: {} }
			],
			edges: [{ fromNodeId: 'a', fromPort: 0, toNodeId: 'b', toPort: 0 }]
		};

		expect(printDsl(contents, registry)).toBe('a = ImportDEM()\nb = Blur(a)\n');
	});

	test('an empty graph prints as nothing', () => {
		expect(printDsl({ nodes: [], edges: [] }, registry)).toBe('');
	});
});

describe('round-trips', () => {
	test('a chain survives text and back', () => {
		const text =
			'dem    = ImportDEM(path: "a.massifdem")\neroded = HydraulicErosion(dem, capacity: 2.5)\n';
		expect(roundTrip(text)).toBe(text);
	});

	test('a secondary output keeps its port name', () => {
		const text = 'dem  = HydraulicErosion()\nflow = Blur(dem.Flow)\n';
		expect(roundTrip(text)).toBe(text);
		expect(parseDsl(text, registry).edges[0].fromPort).toBe(2);
	});

	test('an unconnected input in the middle keeps its place', () => {
		const text = 'dem = ImportDEM()\nout = HydraulicErosion(_, dem)\n';
		expect(roundTrip(text)).toBe(text);
	});

	test('a trailing unconnected input is simply left off', () => {
		expect(roundTrip('dem = ImportDEM()\nout = Blur(dem, _)\n')).toBe(
			'dem = ImportDEM()\nout = Blur(dem)\n'
		);
	});

	test('an enum reads and writes as its option name', () => {
		const text = 'out = ExportPoints(format: Json)\n';
		expect(roundTrip(text)).toBe(text);
		expect(parseDsl(text, registry).nodes[0].parameters.format).toBe(1);
	});

	test('a boolean parameter survives', () => {
		expect(parseDsl('out = ExportPoints(enabled: false)\n', registry).nodes[0].parameters.enabled).toBe(
			false
		);
	});

	test('every parameter is filled from the registry, written or not', () => {
		const node = parseDsl('a = Blur()\n', registry).nodes[0];
		expect(node.parameters).toEqual({ radius: 50 });
	});
});

describe('reading', () => {
	test('comments and blank lines carry no meaning', () => {
		const parsed = parseDsl('# the source\n\ndem = ImportDEM()  # from the survey\n', registry);
		expect(parsed.nodes).toHaveLength(1);
	});

	test('a statement may run over several lines', () => {
		const parsed = parseDsl('dem = ImportDEM(\n  path: "a.massifdem"\n)\n', registry);
		expect(parsed.nodes[0].parameters.path).toBe('a.massifdem');
	});

	test('digit separators are read and then forgotten', () => {
		expect(roundTrip('a = Blur(radius: 2_000)\n')).toBe('a = Blur(radius: 2000)\n');
	});

	test('escapes in text survive both ways', () => {
		expect(roundTrip('a = ImportDEM(path: "C:\\\\terrain\\\\a.massifdem")\n')).toBe(
			'a = ImportDEM(path: "C:\\\\terrain\\\\a.massifdem")\n'
		);
	});
});

describe('refusals', () => {
	test('an unknown type points at plugins', () => {
		expect(errorFrom('a = Nonesuch()\n').message).toContain('plugin');
	});

	test('a node used before it is defined is unknown, so a cycle cannot be written', () => {
		expect(errorFrom('a = Blur(b)\nb = ImportDEM()\n').message).toContain("Unknown node 'b'");
	});

	test('a name may be used only once', () => {
		expect(errorFrom('a = ImportDEM()\na = ImportDEM()\n').message).toContain('twice');
	});

	test('a field will not feed a colour input', () => {
		const message = errorFrom(
			'a = ImportDEM()\nc = ColorRamp(a)\nd = ColorRamp(c)\n'
		).message;
		expect(message).toContain('produces color');
	});

	test('an unknown parameter names the type it was asked of', () => {
		expect(errorFrom('a = Blur(nonsense: 1)\n').message).toContain(
			"Blur has no parameter 'nonsense'"
		);
	});

	test('an unknown output port is refused', () => {
		expect(errorFrom('a = ImportDEM()\nb = Blur(a.Wear)\n').message).toContain(
			"ImportDEM has no output 'Wear'"
		);
	});

	test('more inputs than the node has is refused', () => {
		expect(errorFrom('a = ImportDEM()\nb = ColorRamp(a, a)\n').message).toContain('takes 1 inputs');
	});

	test('an input after a named parameter is refused', () => {
		expect(errorFrom('a = ImportDEM()\nb = Blur(radius: 2, a)\n').message).toContain(
			'Inputs come before named parameters'
		);
	});

	test('a refusal carries the line it happened on', () => {
		expect(errorFrom('a = ImportDEM()\n\nb = Nonesuch()\n').line).toBe(3);
	});

	test('a number where text belongs is refused', () => {
		expect(errorFrom('a = ImportDEM(path: 3)\n').message).toContain('Expected quoted text');
	});
});
