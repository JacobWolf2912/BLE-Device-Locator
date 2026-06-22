namespace DeviceLocator.Api.Mqtt.Payloads;

public record DeviceLocationPayload(
    string DeviceName,
    int Rssi,
    double Distance,
    string? Status,
    string? Timestamp);
