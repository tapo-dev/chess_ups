using System;
using System.Text.RegularExpressions;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// The login panel: server address, port and nickname, a Connect button and a status line.
/// It only checks the input and reports it through <see cref="ConnectRequested"/>. Connecting and
/// logging in are the Session's job, which also tells the panel what to show.
/// </summary>
public class LoginView : MonoBehaviour {
    [SerializeField] TMP_InputField addressField;
    [SerializeField] TMP_InputField portField;
    [SerializeField] TMP_InputField nickField;
    [SerializeField] Button connectButton;
    [SerializeField] TMP_Text statusText;

    [SerializeField] Color infoColor = new Color32(0xAE, 0xB4, 0xBE, 0xFF);
    [SerializeField] Color errorColor = new Color32(0xF2, 0x77, 0x7A, 0xFF);

    // Same rule as the server: 3-16 characters [A-Za-z0-9_].
    // \z rather than $, because $ also matches before a trailing '\n'.
    static readonly Regex NickPattern = new Regex(@"^[A-Za-z0-9_]{3,16}\z");

    /// <summary>Raised with a checked address, port and nickname when the player asks to connect.</summary>
    public event Action<string, int, string> ConnectRequested;

    void Awake() {
        connectButton.onClick.AddListener(Submit);
        // Enter in any of the fields works like the button.
        addressField.onSubmit.AddListener(_ => Submit());
        portField.onSubmit.AddListener(_ => Submit());
        nickField.onSubmit.AddListener(_ => Submit());
        ShowStatus("");
    }

    void Submit() {
        if (!connectButton.interactable)
            return; // the form is locked while a connection attempt is running

        string address = addressField.text.Trim();
        if (Uri.CheckHostName(address) == UriHostNameType.Unknown) {
            ShowError("Enter a valid server address (IP or hostname).");
            return;
        }

        if (!int.TryParse(portField.text.Trim(), out int port) || port < 1 || port > 65535) {
            ShowError("Port must be a number from 1 to 65535.");
            return;
        }

        string nick = nickField.text.Trim();
        if (!NickPattern.IsMatch(nick)) {
            ShowError("Nickname must be 3-16 letters, digits or _.");
            return;
        }

        ConnectRequested?.Invoke(address, port, nick);
    }

    /// <summary>Shows a neutral message, e.g. "Connecting...".</summary>
    public void ShowStatus(string text) {
        statusText.color = infoColor;
        statusText.text = text;
    }

    /// <summary>Shows an error, e.g. a refused connection or ERR INVALID_NICK.</summary>
    public void ShowError(string text) {
        statusText.color = errorColor;
        statusText.text = text;
    }

    /// <summary>Locks the form while connecting and waiting for the LOGIN reply.</summary>
    public void SetInteractable(bool interactable) {
        addressField.interactable = interactable;
        portField.interactable = interactable;
        nickField.interactable = interactable;
        connectButton.interactable = interactable;
    }
}