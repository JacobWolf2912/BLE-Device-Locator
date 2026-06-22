namespace DeviceLocator.Api.Models;

public class DeviceLocation
{
    public long Id { get; set; }

    public required string DeviceName { get; set; }

    public DateTime Timestamp { get; set; }

    public int Rssi { get; set; } // Signal strength in dBm

    public double Distance { get; set; } // Distance in meters

    public string? Status { get; set; } // Connected, Disconnected, etc.
}
