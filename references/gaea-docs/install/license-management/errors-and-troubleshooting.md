Source: https://docs.gaea.app/install/license-management/errors-and-troubleshooting.html

# Errors and Troubleshooting

## Ensure only only one license exists.

Ensure only one `.lic` file of any kind is present in the [DATA Folder](https://docs.gaea.app/using/advanced-topics/technical-information/paths-and-storage.html). Multiple license files can confuse Gaea depending on the order in which they're loaded.

## Firewall Access

Make sure `*.quadspinner.com`, `*.gaea.app` (all subdomains) are allowed TCP access via Firewall or other security apparatus. This should include both port `80` (normal) and port `443` (secure).

## Deactivate / Revoke Activation, then reactivate

If your license is stuck for some reason, you can go to your [User Account](https://quadspinner.com/account) and revoke the activation for a specific. You can then reload the license on the same machine to forcefully refresh the license.

## Hardware changes

If you have an offline node-locked license, large scale hardware changes such as motherboard, processor, etc. can cause a license to become invalid. If your license was issued manually by QuadSpinner, contact Tech Support with a new [Hardware Thumbprint](https://docs.gaea.app/install/license-management/hardware-thumbprint.html) to get a fresh license.

## Other issues

Please file a support ticket using [Diagnostics](https://docs.gaea.app/using/troubleshooting/diagnostics-watson/index.html).
