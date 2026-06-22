using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using DeviceLocator.Api.Data;
using DeviceLocator.Api.Models;
using DeviceLocator.Api.Services;

namespace DeviceLocator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class AuthController(IAuthService authService, AppDbContext db, ILogger<AuthController> logger) : ControllerBase
{
    [HttpPost("seed-testuser")]
    public async Task<ActionResult<string>> SeedTestUser()
    {
        var existingUser = await db.Users.FirstOrDefaultAsync(u => u.Username == "testuser");
        if (existingUser != null)
        {
            return Ok("Test user already exists");
        }

        var testUser = new User
        {
            Username = "testuser",
            PasswordHash = authService.HashPassword("password123"),
            CreatedAt = DateTime.UtcNow
        };

        db.Users.Add(testUser);
        await db.SaveChangesAsync();
        logger.LogInformation("Test user created");

        return Ok("Test user created successfully");
    }

    [HttpPost("login")]
    public async Task<ActionResult<LoginResponse>> Login([FromBody] LoginRequest request)
    {
        if (string.IsNullOrWhiteSpace(request.Username))
            return BadRequest("Username is required");

        if (string.IsNullOrWhiteSpace(request.Password))
            return BadRequest("Password is required");

        logger.LogInformation("Login attempt for user {Username}", request.Username);

        var user = await db.Users.FirstOrDefaultAsync(u => u.Username == request.Username);
        if (user == null)
        {
            logger.LogWarning("Login failed: user {Username} not found", request.Username);
            return Unauthorized("Invalid username or password");
        }

        if (!authService.VerifyPassword(request.Password, user.PasswordHash))
        {
            logger.LogWarning("Login failed: invalid password for user {Username}", request.Username);
            return Unauthorized("Invalid username or password");
        }

        var token = authService.GenerateToken(user.Username);

        logger.LogInformation("Login successful for user {Username}", request.Username);

        return Ok(new LoginResponse
        {
            Token = token,
            Username = user.Username,
            ExpiresIn = 3600
        });
    }
}

public record LoginRequest(string Username, string Password);

public record LoginResponse
{
    public required string Token { get; init; }
    public required string Username { get; init; }
    public required int ExpiresIn { get; init; }
}
