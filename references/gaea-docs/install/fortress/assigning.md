Source: https://docs.gaea.app/install/fortress/assigning.html

# Assign Licenses

Each workstation running Gaea in **offline floating** mode requires a `floating.lic` file to authenticate and communicate with the Fortress server.

See [Paths and Storage](https://docs.gaea.app/using/advanced-topics/technical-information/paths-and-storage.html) for folder locations.

## License File for Workstations

Use **Create License File** in the Fortress Console to generate a workstation license file.

A typical `floating.lic` looks like this:

```
FL-00000000-0000-0000-0000-000000000000
192.168.100.11:9000
```

- Line 1: Floating license key
- Line 2: Fortress server address and port

## Create a License File

1. Select **Create a License File**.

   ![](https://docs.gaea.app/.data/ui/fortress-create1.png)
2. Choose a **Tenant**.

   ![](https://docs.gaea.app/.data/ui/fortress-create2.png)
3. Choose the desired IP address (or enter one manually).

   - If the server has multiple network adapters, all assigned IP addresses will be listed.

   ![](https://docs.gaea.app/.data/ui/fortress-create3.png)
4. Choose where to save the file (or press Enter to accept the default path shown).

   ![](https://docs.gaea.app/.data/ui/fortress-create4.png)

The file is typically saved as `000000.lic` (or using the Tenant’s ID). Copy this file to the [Gaea Data folder](https://docs.gaea.app/using/advanced-topics/technical-information/paths-and-storage.html) on the workstation and rename it to:

- `floating.lic`

Gaea will detect `floating.lic` automatically and load both the license key and server connection details from it.

## Heartbeat Length

- The server uses a 1-minute heartbeat with a 5-minute grace period to tolerate lost packets or temporary network issues.
- If heartbeats are missed for 5 minutes, the license is released.
- An administrator can also use the dashboard to kill an active license if it is stuck.

During a run, a local token is stored so `Gaea.exe`, `Gaea.Build.exe`, and Gaea plugins can share the same token. This prevents over-allocation and other license issues.

## View and Deallocate License Tokens

By default, the Fortress Console shows only Tenants and does not auto-refresh to avoid unnecessary server load.

![](https://docs.gaea.app/.data/ui/fortress-allocs.png)

Select **View Allocations** to see a live list of allocated licenses.

![](https://docs.gaea.app/.data/ui/fortress-allocs-kill.png)

To deallocate a license, select the target machine. All sessions on that machine will be released.

![](https://docs.gaea.app/.data/ui/fortress-allocs-killing.png)

Two deallocation options are available:

- **D (Graceful)**: Sends an "exit gracefully" command to the Gaea instance, then waits 10 seconds for it to be accepted and executed.
- **K (Force Kill)**: Immediately deallocates the workstation.
