---
title: "Connection Profiles"
slug: "/connection-profiles"
description: "Save a radio, the way to reach it and the settings to apply, and switch between setups with one click in the title bar."
---

Save a radio, the way to reach it and the settings to apply, and switch between setups with one click in the title bar.

A connection profile is useful when you run several setups, for example:

- **MSHV** on a FLEX-6600 over the LAN, loading a Flex Global profile set up for MSHV
- **JTDX-VU** on the same 6600 over SmartLink
- **IC-705 FT8**: an Icom by IP address on 14.074 MHz, USB-D, 10 % power

Connection profiles work with every radio family. They are stored on your computer, not on the radio, so they are separate from FlexRadio's own profiles (see [Profile Management](./profile-management.md)).

## Creating a profile

1. Choose **Profiles → Connection Profiles...**. On macOS the menu is in the menu bar at the top of the screen.
2. Click **New from current connection** to start from the radio you are connected to, or **New** to start blank.
3. Type a **Name**.
4. Pick the radio from the **Radio** list. It lists radios found on your network, radios in your SmartLink list and a radio you are connected to by IP. Picking one fills in **Connect via**, **Radio type**, **Serial** and **Address**.
5. Under **After connecting**, fill in only what the profile should change. Blank fields are left as they are:
   - **Flex Global profile** and **Flex TX profile**: loaded on a FlexRadio, Global first
   - **Frequency**, **Mode** and **RF power**: set on the active slice of any radio
6. Click **Save**.

| Connect via | Needs | Notes |
|-------------|-------|-------|
| **LAN (discovered)** | The radio's serial or its address | Uses discovery, and falls back to the address if the radio isn't discovered |
| **IP address** | The address and radio type | For a networked Icom, or a radio on another subnet or VPN (see [Manual Connection](./manual-connection.md)) |
| **SmartLink** | The radio's serial (pick it from the list) | FlexRadio only; log in to SmartLink first (see [SmartLink Setup](./smartlink-setup.md)) |

## Using profiles in the title bar

Once you have saved one profile, the radio tabs in the title bar show your profiles instead of discovered radios. Your radios are still in the **+** list.

- **Click a profile while nothing is connected** to connect it.
- **Click the profile that is connected** (green) to disconnect. AetherSDR asks first.
- **Click another profile** to switch. AetherSDR asks, disconnects the current radio, connects the new profile and then applies its settings.

The connected tab is the profile you connected with. If you connected some other way, it is the first profile whose radio and route match the session: a LAN profile and a SmartLink profile for the same radio are different profiles.

To go back to plain radio tabs, delete all your connection profiles.

## Troubleshooting

### The profile doesn't connect over SmartLink

1. Make sure you are logged in to SmartLink and the radio appears in the SmartLink list.
2. Edit the profile and pick the radio from the **Radio** list again.

### A setting isn't applied

- **Mode** must be one the radio offers; AetherSDR shows a message in the status bar if it isn't.
- **Flex Global profile** rebuilds the radio's slices and panadapters, so frequency and mode are applied after it.

## See also

- [Profile Management](./profile-management.md)
- [Manual Connection](./manual-connection.md)
- [SmartLink Setup](./smartlink-setup.md)
- [Networked Icom](./networked-icom.md)
