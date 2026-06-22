using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.EntityFrameworkCore;
using Microsoft.IdentityModel.Tokens;
using Mqtt.Controllers;
using System.Text;
using DeviceLocator.Api.Data;
using DeviceLocator.Api.Models;
using DeviceLocator.Api.Services;

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.
builder.Services.AddOpenApi();

// Add Entity Framework with SQLite
builder.Services.AddDbContext<AppDbContext>(options =>
{
    var connectionString = builder.Configuration.GetConnectionString("DefaultConnection");
    options.UseSqlite(connectionString);
});

builder.Services.AddCors(options =>
{
    options.AddPolicy("AllowAll", policyBuilder =>
    {
        policyBuilder.AllowAnyOrigin()
            .AllowAnyMethod()
            .AllowAnyHeader();
    });
});

// Add authentication services
var jwtSecretKey = builder.Configuration["Jwt:SecretKey"] ?? "your-super-secret-key-change-this-in-production";
var key = Encoding.ASCII.GetBytes(jwtSecretKey);

builder.Services.AddAuthentication(options =>
{
    options.DefaultAuthenticateScheme = JwtBearerDefaults.AuthenticationScheme;
    options.DefaultChallengeScheme = JwtBearerDefaults.AuthenticationScheme;
})
.AddJwtBearer(options =>
{
    options.TokenValidationParameters = new TokenValidationParameters
    {
        ValidateIssuerSigningKey = true,
        IssuerSigningKey = new SymmetricSecurityKey(key),
        ValidateIssuer = true,
        ValidIssuer = builder.Configuration["Jwt:Issuer"] ?? "DeviceLocator",
        ValidateAudience = true,
        ValidAudience = builder.Configuration["Jwt:Audience"] ?? "DeviceLocatorUsers",
        ValidateLifetime = true,
        ClockSkew = TimeSpan.Zero
    };
});

builder.Services.AddAuthorization();

// Add Auth Service
builder.Services.AddScoped<IAuthService, AuthService>();

// Add MQTT Controllers
builder.Services.AddMqttControllers();

// Add REST Controllers
builder.Services.AddControllers();


var app = builder.Build();

// Connect to MQTT broker with retry logic
bool mqttConnected = false;
int retries = 0;
int maxRetries = 5;

while (!mqttConnected && retries < maxRetries)
{
    try
    {
        var mqtt = app.Services.GetRequiredService<IMqttClientService>();
        app.Logger.LogInformation("Attempting MQTT connection (attempt {Attempt}/{MaxAttempts})", retries + 1, maxRetries);
        await mqtt.ConnectAsync("broker.hivemq.com", 1883);
        app.Logger.LogInformation("Connected to MQTT broker at broker.hivemq.com:1883");
        mqttConnected = true;
    }
    catch (Exception ex)
    {
        retries++;
        if (retries < maxRetries)
        {
            int delayMs = 3000 * retries; // Increase delay: 3s, 6s, 9s, 12s, 15s
            app.Logger.LogWarning(ex, "MQTT connection failed. Retrying in {DelaySeconds} seconds... (attempt {Attempt}/{MaxAttempts})",
                delayMs / 1000, retries + 1, maxRetries);
            await Task.Delay(delayMs);
        }
        else
        {
            app.Logger.LogError(ex, "Failed to connect to MQTT broker after {MaxRetries} attempts. Continuing without MQTT...", maxRetries);
        }
    }
}

// Initialize database
using (var scope = app.Services.CreateScope())
{
    var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();
    var authService = scope.ServiceProvider.GetRequiredService<IAuthService>();
    try
    {
        db.Database.Migrate();
        app.Logger.LogInformation("Database migrated successfully");

        // Seed test user if it doesn't exist
        var existingUser = db.Users.FirstOrDefault(u => u.Username == "testuser");
        if (existingUser == null)
        {
            var testUser = new User
            {
                Username = "testuser",
                PasswordHash = authService.HashPassword("password123"),
                CreatedAt = DateTime.UtcNow
            };
            db.Users.Add(testUser);
            db.SaveChanges();
            app.Logger.LogInformation("Test user 'testuser' created with password 'password123'");
        }
    }
    catch (Exception ex)
    {
        app.Logger.LogError(ex, "Failed to initialize database");
        throw;
    }
}

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.MapOpenApi();
}

if (!app.Environment.IsDevelopment())
{
    app.UseHttpsRedirection();
}

app.UseDefaultFiles();
app.UseStaticFiles();

app.UseCors("AllowAll");

app.UseAuthentication();
app.UseAuthorization();

// Map REST controllers
app.MapControllers();

// Fallback for SPA routing - serve index.html for non-API routes
app.MapFallback(async context =>
{
    if (!context.Request.Path.StartsWithSegments("/api"))
    {
        var env = app.Services.GetRequiredService<IWebHostEnvironment>();
        var filePath = Path.Combine(env.WebRootPath, "index.html");
        context.Response.ContentType = "text/html";
        await context.Response.SendFileAsync(filePath);
    }
});

await app.RunAsync();
