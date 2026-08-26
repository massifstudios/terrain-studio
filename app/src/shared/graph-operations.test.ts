import { describe, expect, test } from 'bun:test';
import type {
	GraphEdgeDocument,
	GraphNodeDocument,
	NodeSchema,
	PortSchema,
	RegistrySchema
} from './graph-document';
import {
	canConnect,
	nextNodeId,
	reachesDownstream,
	withConnection,
	withMovedNodes,
	withoutInput,
	withoutNodes,
	withParameter
} from './graph-operations';

function port(name: string, type: PortSchema['type']): PortSchema {
	return { name, description: '', type, fieldSemantics: 'heightfield', optional: false };
}

function schema(
	typeName: string,
	inputs: PortSchema[],
	outputs: PortSchema[]
): NodeSchema {
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
		schema('MixColor', [port('A', 'color')], [port('Out', 'color')])
	]
};

function node(id: string, typeName: string): GraphNodeDocument {
	return { id, typeName, positionX: 0, positionY: 0, parameters: {} };
}

function chain(): { nodes: GraphNodeDocument[]; edges: GraphEdgeDocument[] } {
	return {
		nodes: [node('constant1', 'Constant'), node('blur1', 'Blur'), node('blur2', 'Blur')],
		edges: [
			{ fromNodeId: 'constant1', fromPort: 0, toNodeId: 'blur1', toPort: 0 },
			{ fromNodeId: 'blur1', fromPort: 0, toNodeId: 'blur2', toPort: 0 }
		]
	};
}

describe('node ids', () => {
	test('are derived from the type name and stay unique', () => {
		const nodes = [node('blur1', 'Blur'), node('blur2', 'Blur')];
		expect(nextNodeId(nodes, 'Blur')).toBe('blur3');
		expect(nextNodeId(nodes, 'ImportDEM')).toBe('importDEM1');
	});
});

describe('reachability', () => {
	test('follows edges downstream', () => {
		const { edges } = chain();
		expect(reachesDownstream(edges, 'constant1', 'blur2')).toBe(true);
		expect(reachesDownstream(edges, 'blur2', 'constant1')).toBe(false);
	});
});

describe('connections', () => {
	test('matching port types connect', () => {
		const { nodes, edges } = chain();
		expect(
			canConnect(registry, nodes, edges, {
				fromNodeId: 'constant1',
				fromPort: 0,
				toNodeId: 'blur2',
				toPort: 0
			})
		).toBe(true);
	});

	test('a field output will not feed a colour input', () => {
		const nodes = [node('constant1', 'Constant'), node('mixColor1', 'MixColor')];
		expect(
			canConnect(registry, nodes, [], {
				fromNodeId: 'constant1',
				fromPort: 0,
				toNodeId: 'mixColor1',
				toPort: 0
			})
		).toBe(false);
	});

	test('a connection that would close a loop is refused', () => {
		const { nodes, edges } = chain();
		expect(
			canConnect(registry, nodes, edges, {
				fromNodeId: 'blur2',
				fromPort: 0,
				toNodeId: 'blur1',
				toPort: 0
			})
		).toBe(false);
	});

	test('a node will not connect to itself', () => {
		const { nodes, edges } = chain();
		expect(
			canConnect(registry, nodes, edges, {
				fromNodeId: 'blur1',
				fromPort: 0,
				toNodeId: 'blur1',
				toPort: 0
			})
		).toBe(false);
	});

	test('connecting an occupied input replaces the edge that was there', () => {
		const { edges } = chain();
		const replaced = withConnection(edges, {
			fromNodeId: 'constant1',
			fromPort: 0,
			toNodeId: 'blur2',
			toPort: 0
		});

		expect(replaced.filter((edge) => edge.toNodeId === 'blur2')).toHaveLength(1);
		expect(replaced.find((edge) => edge.toNodeId === 'blur2')?.fromNodeId).toBe('constant1');
	});

	test('disconnecting an input leaves the rest alone', () => {
		const { edges } = chain();
		expect(withoutInput(edges, 'blur1', 0)).toHaveLength(1);
	});
});

describe('deletion', () => {
	test('removing a node removes the edges that touched it', () => {
		const contents = chain();
		const remaining = withoutNodes(contents, ['blur1']);

		expect(remaining.nodes.map((entry) => entry.id)).toEqual(['constant1', 'blur2']);
		expect(remaining.edges).toHaveLength(0);
	});
});

describe('editing', () => {
	test('moving affects only the nodes named', () => {
		const contents = chain();
		const moved = withMovedNodes(contents.nodes, ['blur1'], 10, -5);

		expect(moved.find((entry) => entry.id === 'blur1')?.positionX).toBe(10);
		expect(moved.find((entry) => entry.id === 'blur1')?.positionY).toBe(-5);
		expect(moved.find((entry) => entry.id === 'blur2')?.positionX).toBe(0);
	});

	test('setting a parameter leaves the other nodes untouched', () => {
		const contents = chain();
		const edited = withParameter(contents.nodes, 'blur1', 'radius', 120);

		expect(edited.find((entry) => entry.id === 'blur1')?.parameters.radius).toBe(120);
		expect(edited.find((entry) => entry.id === 'blur2')?.parameters).toEqual({});
	});
});
