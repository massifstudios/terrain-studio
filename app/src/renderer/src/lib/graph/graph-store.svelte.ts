import type {
	GraphDocument,
	GraphEdgeDocument,
	GraphNodeDocument,
	GraphSettings,
	NodeSchema,
	ParameterValue,
	RegistrySchema
} from '$shared/graph-document';
import {
	canConnect,
	findNode,
	nextNodeId,
	withConnection,
	withMovedNodes,
	withoutInput,
	withoutNodes,
	withParameter,
	type ConnectionRequest
} from '$shared/graph-operations';
import type { GraphProject } from '$shared/massifgraph';
import { defaultParameters } from './registry-schema';

function emptySettings(): GraphSettings {
	return {
		resolution: 512,
		worldSizeMeters: 10000,
		seed: 1,
		georeference: { coordinateSystem: '', eastingMinMeters: 0, northingMaxMeters: 0 }
	};
}

/**
 * The single source of truth for the graph. The node editor renders this and
 * emits mutations; no editor component holds graph state of its own. Every
 * rule lives in `$shared/graph-operations`, which this only makes reactive.
 */
class GraphStore {
	settings = $state<GraphSettings>(emptySettings());
	nodes = $state<GraphNodeDocument[]>([]);
	edges = $state<GraphEdgeDocument[]>([]);
	selectedNodeIds = $state<string[]>([]);
	previewNodeId = $state<string | null>(null);
	filePath = $state<string | null>(null);
	hasUnsavedChanges = $state(false);
	/// A session preference, not part of the project: what a Build would use.
	buildResolution = $state(8192);

	document(): GraphDocument {
		return {
			settings: $state.snapshot(this.settings),
			nodes: $state.snapshot(this.nodes),
			edges: $state.snapshot(this.edges)
		};
	}

	project(): GraphProject {
		return { graph: this.document(), previewNodeId: this.previewNodeId };
	}

	findNode(nodeId: string): GraphNodeDocument | null {
		return findNode(this.nodes, nodeId);
	}

	get selectedNode(): GraphNodeDocument | null {
		if (this.selectedNodeIds.length !== 1) {
			return null;
		}
		return this.findNode(this.selectedNodeIds[0]);
	}

	addNode(schema: NodeSchema, positionX: number, positionY: number): string {
		const id = nextNodeId(this.nodes, schema.typeName);
		this.nodes = [
			...this.nodes,
			{ id, typeName: schema.typeName, positionX, positionY, parameters: defaultParameters(schema) }
		];
		this.select([id]);
		this.#touch();
		return id;
	}

	moveNodes(nodeIds: string[], deltaX: number, deltaY: number): void {
		this.nodes = withMovedNodes(this.nodes, nodeIds, deltaX, deltaY);
		this.#touch();
	}

	removeNodes(nodeIds: string[]): void {
		const remaining = withoutNodes({ nodes: this.nodes, edges: this.edges }, nodeIds);
		this.nodes = remaining.nodes;
		this.edges = remaining.edges;

		const removing = new Set(nodeIds);
		this.selectedNodeIds = this.selectedNodeIds.filter((id) => !removing.has(id));
		if (this.previewNodeId !== null && removing.has(this.previewNodeId)) {
			this.previewNodeId = null;
		}
		this.#touch();
	}

	canConnect(registry: RegistrySchema | null, request: ConnectionRequest): boolean {
		return canConnect(registry, this.nodes, this.edges, request);
	}

	connect(request: ConnectionRequest): void {
		this.edges = withConnection(this.edges, request);
		this.#touch();
	}

	disconnectInput(toNodeId: string, toPort: number): void {
		this.edges = withoutInput(this.edges, toNodeId, toPort);
		this.#touch();
	}

	setParameter(nodeId: string, name: string, value: ParameterValue): void {
		this.nodes = withParameter(this.nodes, nodeId, name, value);
		this.#touch();
	}

	select(nodeIds: string[]): void {
		this.selectedNodeIds = nodeIds;
		if (nodeIds.length === 1) {
			this.previewNodeId = nodeIds[0];
		}
	}

	load(project: GraphProject, filePath: string | null): void {
		this.settings = project.graph.settings;
		this.nodes = project.graph.nodes;
		this.edges = project.graph.edges;
		this.previewNodeId = project.previewNodeId;
		this.selectedNodeIds = [];
		this.filePath = filePath;
		this.hasUnsavedChanges = false;
	}

	markSaved(filePath: string): void {
		this.filePath = filePath;
		this.hasUnsavedChanges = false;
	}

	#touch(): void {
		this.hasUnsavedChanges = true;
	}
}

export const graphStore = new GraphStore();
