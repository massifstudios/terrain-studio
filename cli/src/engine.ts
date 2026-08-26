/**
 * The engine without a window. This loads the very binary the app's
 * utilityProcess loads, and nothing here knows that Electron exists.
 */

import { createRequire } from 'node:module';
import { delimiter, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import type { EngineAddon } from '$shared/engine-addon';
import type { RegistrySchema } from '$shared/graph-document';

const repositoryRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');

function addonPath(): string {
	const configured = process.env.TERRAIN_ENGINE_ADDON;
	if (configured) {
		return configured;
	}
	return join(repositoryRoot, 'bridge', 'build', 'Release', 'engine.node');
}

/// Which directories hold plugins is the caller's decision, exactly as it is
/// the main process's decision in the app.
function pluginDirectories(): string[] {
	const configured = process.env.TERRAIN_PLUGIN_PATH;
	if (!configured) {
		return [];
	}
	return configured.split(delimiter).filter((entry) => entry.length > 0);
}

/// A refused plugin is reported and never swallowed, but it does not stop the
/// command: a graph that needs none of its nodes still builds.
function reportPlugins(addon: EngineAddon): void {
	for (const plugin of addon.loadPlugins(pluginDirectories())) {
		if (plugin.message) {
			console.error(`Plugin ${plugin.path} ${plugin.message}`);
		}
	}
}

export function openEngine(): EngineAddon {
	const addon = createRequire(import.meta.url)(addonPath()) as EngineAddon;
	reportPlugins(addon);
	return addon;
}

export function readRegistry(addon: EngineAddon): RegistrySchema {
	return JSON.parse(addon.nodeRegistryJson()) as RegistrySchema;
}
