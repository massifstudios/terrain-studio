import { describe, expect, test } from 'bun:test';
import type { GraphProject } from './massifgraph';
import { massifGraphVersion, parseGraph, serializeGraph } from './massifgraph';

function sampleProject(): GraphProject {
	return {
		graph: {
			settings: {
				resolution: 1024,
				worldSizeMeters: 20000,
				seed: 1337,
				georeference: {
					coordinateSystem: 'EPSG:31254',
					eastingMinMeters: 39075.5,
					northingMaxMeters: 214292.25
				}
			},
			nodes: [
				{
					id: 'sum',
					typeName: 'Add',
					positionX: 420,
					positionY: 60,
					parameters: {}
				},
				{
					id: 'source',
					typeName: 'ImportDEM',
					positionX: 40,
					positionY: 20,
					parameters: { path: 'oetztal.massifdem', minimumAltitude: 0, maximumAltitude: 3000 }
				},
				{
					id: 'lift',
					typeName: 'Constant',
					positionX: 40,
					positionY: 220,
					parameters: { value: 25 }
				}
			],
			edges: [
				{ fromNodeId: 'lift', fromPort: 0, toNodeId: 'sum', toPort: 1 },
				{ fromNodeId: 'source', fromPort: 0, toNodeId: 'sum', toPort: 0 }
			]
		},
		previewNodeId: 'sum'
	};
}

describe('massifgraph', () => {
	test('a project survives a save and a load', () => {
		const original = sampleProject();
		const reloaded = parseGraph(serializeGraph(original));

		expect(reloaded.previewNodeId).toBe('sum');
		expect(reloaded.graph.settings).toEqual(original.graph.settings);
		expect(reloaded.graph.nodes).toHaveLength(3);
		expect(reloaded.graph.edges).toHaveLength(2);

		const source = reloaded.graph.nodes.find((node) => node.id === 'source');
		expect(source?.typeName).toBe('ImportDEM');
		expect(source?.parameters.path).toBe('oetztal.massifdem');
		expect(source?.positionX).toBe(40);
	});

	test('serialising twice gives the same bytes', () => {
		const project = sampleProject();
		expect(serializeGraph(project)).toBe(serializeGraph(parseGraph(serializeGraph(project))));
	});

	test('node and edge order does not depend on the order they were made', () => {
		const project = sampleProject();
		const shuffled: GraphProject = {
			previewNodeId: project.previewNodeId,
			graph: {
				settings: project.graph.settings,
				nodes: [...project.graph.nodes].reverse(),
				edges: [...project.graph.edges].reverse()
			}
		};

		expect(serializeGraph(shuffled)).toBe(serializeGraph(project));
	});

	test('every key in the file is snake_case', () => {
		const text = serializeGraph(sampleProject());

		expect(text).toContain('"world_size_meters"');
		expect(text).toContain('"coordinate_system"');
		expect(text).toContain('"northing_max_meters"');
		expect(text).toContain('"preview_node_id"');
		expect(text).toContain('"from_node_id"');
		expect(text).toContain('"to_port"');
		expect(text).toContain('"minimum_altitude"');
	});

	test('a node is written as type, x and y', () => {
		const text = serializeGraph(sampleProject());

		expect(text).toContain('"type": "ImportDEM"');
		expect(text).toContain('"x": 420');
		expect(text).toContain('"y": 60');
		expect(text).not.toContain('"typeName"');
		expect(text).not.toContain('"positionX"');
	});

	test('a camelCase parameter name comes back as it went in', () => {
		const project = sampleProject();
		project.graph.nodes[0].parameters = { useGPU: true, noise2D: 4, talusAngle: 34 };
		const text = serializeGraph(project);

		expect(text).toContain('"use_g_p_u"');
		expect(text).toContain('"noise2_d"');
		expect(text).toContain('"talus_angle"');

		const reloaded = parseGraph(text).graph.nodes.find((node) => node.id === 'sum');
		expect(reloaded?.parameters).toEqual({ useGPU: true, noise2D: 4, talusAngle: 34 });
	});

	test('parameter keys come out sorted, whatever order they went in', () => {
		const project = sampleProject();
		const text = serializeGraph(project);
		const parameterBlock = text.slice(text.indexOf('"maximum_altitude"'));

		expect(parameterBlock.indexOf('"minimum_altitude"')).toBeGreaterThan(0);
		expect(parameterBlock.indexOf('"path"')).toBeGreaterThan(
			parameterBlock.indexOf('"minimum_altitude"')
		);
	});

	test('the file states its version', () => {
		expect(serializeGraph(sampleProject())).toContain(`"version": ${massifGraphVersion}`);
	});

	test('a file from a future version is refused', () => {
		const text = serializeGraph(sampleProject()).replace('"version": 1', '"version": 99');
		expect(() => parseGraph(text)).toThrow('Unsupported .massifgraph version 99');
	});

	test('a file with no version at all is refused', () => {
		expect(() => parseGraph('{"nodes": [], "edges": []}')).toThrow(
			'A .massifgraph must state its version'
		);
	});

	test('a field of the wrong type is refused, and the message says which', () => {
		const text = serializeGraph(sampleProject()).replace('"resolution": 1024', '"resolution": "big"');
		expect(() => parseGraph(text)).toThrow('settings.resolution');
	});

	test('a node missing its type is refused', () => {
		const text = serializeGraph(sampleProject()).replace('"type": "Add",', '');
		expect(() => parseGraph(text)).toThrow('nodes.2.type');
	});
});
