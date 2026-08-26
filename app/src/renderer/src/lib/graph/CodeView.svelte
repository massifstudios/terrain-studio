<!--
	The graph as text. The graph itself stays the single source of truth: this
	prints it, and parses edits straight back into it. While the box has focus
	the typed text is left exactly as typed; leaving it prints the graph again,
	which is where the text gets tidied.

	A plain textarea rather than a design-system control: the system has no
	multi-line text field, and `Select`, `Checkbox` and `ListRow` are all
	single-value controls. Type, colour and spacing come from its tokens.
-->
<script lang="ts">
	import { engineClient } from '$lib/engine-client.svelte';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import { DslError, parseDsl, printDsl } from '$shared/graph-dsl';
	import { laidOut } from '$shared/graph-layout';

	let draft = $state('');
	let complaint = $state<string | null>(null);
	let isEditing = $state(false);

	const printed = $derived.by(() => {
		const registry = engineClient.registry;
		if (!registry) {
			return '';
		}
		return printDsl({ nodes: graphStore.nodes, edges: graphStore.edges }, registry);
	});

	$effect(() => {
		const text = printed;
		if (isEditing) {
			return;
		}
		draft = text;
		complaint = null;
	});

	function describe(error: unknown): string {
		if (error instanceof DslError) {
			return error.message;
		}
		if (error instanceof Error) {
			return error.message;
		}
		return String(error);
	}

	function apply(text: string): void {
		draft = text;
		const registry = engineClient.registry;
		if (!registry) {
			complaint = 'The engine has not reported its nodes yet';
			return;
		}
		try {
			const parsed = parseDsl(text, registry);
			graphStore.replaceContents(laidOut(parsed, registry, graphStore.nodes), parsed.edges);
			complaint = null;
		} catch (error) {
			complaint = describe(error);
		}
	}
</script>

<div class="flex h-full flex-col">
	<textarea
		class="min-h-0 flex-1 resize-none bg-canvas px-4 py-3 font-mono text-2xs leading-relaxed text-default outline-none"
		spellcheck="false"
		autocomplete="off"
		placeholder="dem = ImportDEM(path: &quot;terrain.massifdem&quot;)"
		value={draft}
		onfocus={() => (isEditing = true)}
		onblur={() => (isEditing = false)}
		oninput={(event) => apply(event.currentTarget.value)}
	></textarea>

	{#if complaint}
		<div class="shrink-0 border-t border-line px-4 py-2 text-2xs text-red">{complaint}</div>
	{/if}
</div>
