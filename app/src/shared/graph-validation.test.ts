import { describe, expect, test } from 'bun:test';
import type {
	GraphDocument,
	GraphEdgeDocument,
	GraphNodeDocument,
	NodeSchema,
	PortSchema,
	RegistrySchema
} from './graph-document';
import { emptyGraphSettings } from './graph-document';
import { graphComplaints } from './graph-validation';

function port(name: string, type: PortSchema['type']): PortSchema {
	return { name, description: '', type, fieldSemantics: 'heightfield', optional: false };
}

function schema(typeName: string, inputs: PortSchema[], outputs: PortSchema[]): NodeSchema {
	return {
		typeName,
		category: 'Test',
		description: '',
		version: 1,
		inputs,
		outputs,
		parameters: []
	};
}

const registry: RegistrySchema = {
	abiVersion: 1,
	nodes: [
		schema('Constant', [], [port('Out', 'field')]),
		schema('Blur', [port('In', 'field')], [port('Out', 'field')]),
		schema('ColorRamp', [port('In', 'field')], [port('Out', 'color')]),
		schema('MixColor', [port('A', 'color'), port('B', 'color')], [port('Out', 'color')])
	]
};

function node(id: string, typeName: string): GraphNodeDocument {
	return { id, typeName, positionX: 0, positionY: 0, parameters: {} };
}

function graphOf(nodes: GraphNodeDocument[], edges: GraphEdgeDocument[]): GraphDocument {
	return { settings: emptyGraphSettings(), nodes, edges };
}

describe('graph validation', () => {
	test('a graph whose types and ports all exist has nothing to say', () => {
		const graph = graphOf(
			[node('constant1', 'Constant'), node('blur1', 'Blur')],
			[{ fromNodeId: 'constant1', fromPort: 0, toNodeId: 'blur1', toPort: 0 }]
		);
		expect(graphComplaints(graph, registry)).toEqual([]);
	});

	test('a node type the registry does not offer names the node', () => {
		const graph = graphOf([node('erosion1', 'PlateTectonics')], []);
		expect(graphComplaints(graph, registry)).toEqual([
			"erosion1: no node type 'PlateTectonics' — is a plugin missing?"
		]);
	});

	test('an edge with an end outside the graph is refused', () => {
		const graph = graphOf(
			[node('blur1', 'Blur')],
			[{ fromNodeId: 'ghost', fromPort: 0, toNodeId: 'blur1', toPort: 0 }]
		);
		expect(graphComplaints(graph, registry)).toEqual([
			'ghost -> blur1: an end of this edge is not in the graph'
		]);
	});

	test('a port index past the end of the node is refused', () => {
		const graph = graphOf(
			[node('constant1', 'Constant'), node('blur1', 'Blur')],
			[{ fromNodeId: 'constant1', fromPort: 0, toNodeId: 'blur1', toPort: 3 }]
		);
		expect(graphComplaints(graph, registry)).toEqual([
			'constant1 -> blur1: this edge uses a port that does not exist'
		]);
	});

	test('a colour output cannot feed a field input', () => {
		const graph = graphOf(
			[node('constant1', 'Constant'), node('ramp1', 'ColorRamp'), node('blur1', 'Blur')],
			[
				{ fromNodeId: 'constant1', fromPort: 0, toNodeId: 'ramp1', toPort: 0 },
				{ fromNodeId: 'ramp1', fromPort: 0, toNodeId: 'blur1', toPort: 0 }
			]
		);
		expect(graphComplaints(graph, registry)).toEqual([
			'ramp1 -> blur1: color cannot feed field'
		]);
	});

	test('every fault is reported, not only the first', () => {
		const graph = graphOf(
			[node('mystery1', 'PlateTectonics'), node('blur1', 'Blur')],
			[{ fromNodeId: 'ghost', fromPort: 0, toNodeId: 'blur1', toPort: 0 }]
		);
		expect(graphComplaints(graph, registry)).toHaveLength(2);
	});
});
