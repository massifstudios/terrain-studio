/**
 * The `.massifgraph` project file: JSON, human-diffable, stable key order.
 *
 * Every key in the file is snake_case, including the parameter names a node
 * carries. The names the registry reports are camelCase, so this module is the
 * one place the two spellings meet — everything upstream of it, in memory and
 * across the engine protocol, stays camelCase.
 *
 * Node types are not enumerated here. A file records whatever type name the
 * registry offered, so a graph saved with a plugin's node still loads — the
 * placeholder handling for a missing plugin lives with the registry, not here.
 */

import { z } from 'zod';
import type {
	GraphDocument,
	GraphEdgeDocument,
	GraphNodeDocument,
	GraphSettings,
	ParameterValue
} from './graph-document';

export const massifGraphVersion = 1;

export interface GraphProject {
	graph: GraphDocument;
	previewNodeId: string | null;
}

/* --------------------------------------------------------------- key case -- */

/**
 * A camelCase name and its snake_case spelling are the same name written two
 * ways, and the conversion is its own inverse: `useGPU` writes as `use_g_p_u`
 * and reads back as `useGPU`. That only holds for names the C++ style rules
 * allow — a parameter name that already contains an underscore would come back
 * camelCased and no longer match the registry.
 */
function toSnakeCase(name: string): string {
	return name.replace(/[A-Z]/g, (letter) => `_${letter.toLowerCase()}`);
}

function toCamelCase(name: string): string {
	return name.replace(/_(.)/g, (_match, letter: string) => letter.toUpperCase());
}

/* ------------------------------------------------------------ the document -- */

const parameterValueSchema = z.union([z.number(), z.string(), z.boolean()]);

const georeferenceSchema = z.object({
	coordinate_system: z.string(),
	easting_min_meters: z.number(),
	northing_max_meters: z.number()
});

const settingsSchema = z.object({
	resolution: z.number(),
	world_size_meters: z.number(),
	seed: z.number(),
	georeference: georeferenceSchema
});

const nodeSchema = z.object({
	id: z.string(),
	type: z.string(),
	x: z.number(),
	y: z.number(),
	parameters: z.record(z.string(), parameterValueSchema)
});

const edgeSchema = z.object({
	from_node_id: z.string(),
	from_port: z.number().int(),
	to_node_id: z.string(),
	to_port: z.number().int()
});

const documentSchema = z.object({
	version: z.number(),
	settings: settingsSchema,
	preview_node_id: z.string().nullable(),
	nodes: z.array(nodeSchema),
	edges: z.array(edgeSchema)
});

type GraphFileDocument = z.infer<typeof documentSchema>;

/* -------------------------------------------------------------- to a file -- */

function writtenParameters(parameters: Record<string, ParameterValue>): Record<string, ParameterValue> {
	const written: Record<string, ParameterValue> = {};
	for (const name of Object.keys(parameters).map(toSnakeCase).sort()) {
		written[name] = parameters[toCamelCase(name)];
	}
	return written;
}

function writtenNode(node: GraphNodeDocument): GraphFileDocument['nodes'][number] {
	return {
		id: node.id,
		type: node.typeName,
		x: node.positionX,
		y: node.positionY,
		parameters: writtenParameters(node.parameters)
	};
}

function writtenEdge(edge: GraphEdgeDocument): GraphFileDocument['edges'][number] {
	return {
		from_node_id: edge.fromNodeId,
		from_port: edge.fromPort,
		to_node_id: edge.toNodeId,
		to_port: edge.toPort
	};
}

function writtenSettings(settings: GraphSettings): GraphFileDocument['settings'] {
	return {
		resolution: settings.resolution,
		world_size_meters: settings.worldSizeMeters,
		seed: settings.seed,
		georeference: {
			coordinate_system: settings.georeference.coordinateSystem,
			easting_min_meters: settings.georeference.eastingMinMeters,
			northing_max_meters: settings.georeference.northingMaxMeters
		}
	};
}

function compareEdges(left: GraphEdgeDocument, right: GraphEdgeDocument): number {
	if (left.toNodeId !== right.toNodeId) {
		return left.toNodeId.localeCompare(right.toNodeId);
	}
	if (left.toPort !== right.toPort) {
		return left.toPort - right.toPort;
	}
	if (left.fromNodeId !== right.fromNodeId) {
		return left.fromNodeId.localeCompare(right.fromNodeId);
	}
	return left.fromPort - right.fromPort;
}

/// Sorted so that moving a node in the editor does not reorder the file.
export function serializeGraph(project: GraphProject): string {
	const nodes = [...project.graph.nodes]
		.sort((left, right) => left.id.localeCompare(right.id))
		.map(writtenNode);
	const edges = [...project.graph.edges].sort(compareEdges).map(writtenEdge);

	return `${JSON.stringify(
		{
			version: massifGraphVersion,
			settings: writtenSettings(project.graph.settings),
			preview_node_id: project.previewNodeId,
			nodes,
			edges
		},
		null,
		'\t'
	)}\n`;
}

/* ------------------------------------------------------------ from a file -- */

function readParameters(parameters: Record<string, ParameterValue>): Record<string, ParameterValue> {
	const read: Record<string, ParameterValue> = {};
	for (const name of Object.keys(parameters).sort()) {
		read[toCamelCase(name)] = parameters[name];
	}
	return read;
}

function readNode(node: GraphFileDocument['nodes'][number]): GraphNodeDocument {
	return {
		id: node.id,
		typeName: node.type,
		positionX: node.x,
		positionY: node.y,
		parameters: readParameters(node.parameters)
	};
}

function readEdge(edge: GraphFileDocument['edges'][number]): GraphEdgeDocument {
	return {
		fromNodeId: edge.from_node_id,
		fromPort: edge.from_port,
		toNodeId: edge.to_node_id,
		toPort: edge.to_port
	};
}

function readSettings(settings: GraphFileDocument['settings']): GraphSettings {
	return {
		resolution: settings.resolution,
		worldSizeMeters: settings.world_size_meters,
		seed: settings.seed,
		georeference: {
			coordinateSystem: settings.georeference.coordinate_system,
			eastingMinMeters: settings.georeference.easting_min_meters,
			northingMaxMeters: settings.georeference.northing_max_meters
		}
	};
}

/// The version is checked on its own so that an old or a future file says so,
/// rather than failing as a pile of unmet field expectations.
function requireSupportedVersion(parsed: unknown): void {
	const found = z.object({ version: z.number() }).safeParse(parsed);
	if (!found.success) {
		throw new Error('A .massifgraph must state its version');
	}
	if (found.data.version !== massifGraphVersion) {
		throw new Error(`Unsupported .massifgraph version ${found.data.version}`);
	}
}

function describeFailure(error: z.ZodError): string {
	const issue = error.issues[0];
	const where = issue.path.join('.');
	if (where.length === 0) {
		return issue.message;
	}
	return `${where}: ${issue.message}`;
}

export function parseGraph(text: string): GraphProject {
	const parsed: unknown = JSON.parse(text);
	requireSupportedVersion(parsed);

	const document = documentSchema.safeParse(parsed);
	if (!document.success) {
		throw new Error(`This is not a valid .massifgraph — ${describeFailure(document.error)}`);
	}

	return {
		graph: {
			settings: readSettings(document.data.settings),
			nodes: document.data.nodes.map(readNode),
			edges: document.data.edges.map(readEdge)
		},
		previewNodeId: document.data.preview_node_id
	};
}
